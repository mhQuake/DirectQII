/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include <D3Dcompiler.h>

// needed for the shader compiler
// pD3DCompile is defined in D3Dcompiler.h so we don't need to typedef it ourselves
HINSTANCE hInstCompiler = NULL;
pD3DCompile QD3DCompile = NULL;

typedef struct shaderbundle_s {
	ID3D11InputLayout *InputLayout;
	ID3D11VertexShader *VertexShader;
	ID3D11GeometryShader *GeometryShader;
	ID3D11PixelShader *PixelShader;
} shaderbundle_t;


#define MAX_SHADERS 1024

static shaderbundle_t d3d_Shaders[MAX_SHADERS];
static int d3d_NumShaders = 0;

// using D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT for compat with feature levels
ID3D11Buffer *d3d_ConstantBuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];


void R_ShutdownShaders (void)
{
	int i;

	// release shaders
	for (i = 0; i < MAX_SHADERS; i++)
	{
		SAFE_RELEASE (d3d_Shaders[i].InputLayout);
		SAFE_RELEASE (d3d_Shaders[i].VertexShader);
		SAFE_RELEASE (d3d_Shaders[i].GeometryShader);
		SAFE_RELEASE (d3d_Shaders[i].PixelShader);
	}

	memset (d3d_Shaders, 0, sizeof (d3d_Shaders));

	// release cBuffers
	for (i = 0; i < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
		SAFE_RELEASE (d3d_ConstantBuffers[i]);

	// these should no longer be used
	QD3DCompile = NULL;

	if (hInstCompiler)
	{
		FreeLibrary (hInstCompiler);
		hInstCompiler = NULL;
	}
}


#define STR_EXPAND(tok) #tok
#define DEFINE_CONSTANT_REGISTER(n) {#n, "c"STR_EXPAND (n)}
#define DEFINE_SAMPLER_REGISTER(n) {#n, "s"STR_EXPAND (n)}


static int LoadResourceData (int resourceid, void **resbuf)
{
	// per MSDN, UnlockResource is obsolete and does nothing any more.  There is
	// no way to free the memory used by a resource after you're finished with it.
	// If you ask me this is kinda fucked, but what do I know?  We'll just leak it.
	if (resbuf)
	{
		HRSRC hResInfo = FindResource (NULL, MAKEINTRESOURCE (resourceid), RT_RCDATA);

		if (hResInfo)
		{
			HGLOBAL hResData = LoadResource (NULL, hResInfo);

			if (hResData)
			{
				resbuf[0] = (byte *) LockResource (hResData);
				return SizeofResource (NULL, hResInfo);
			}
		}
	}

	return 0;
}


void D_LoadShaderCompiler (void)
{
	// statically linking to d3dcompiler.lib causes it to use d3dcompiler_47.dll which is not on Windows 7 so link dynamically instead
	if (!hInstCompiler || !QD3DCompile)
	{
		int i;

		for (i = 99; i > 32; i--)
		{
			char libname[256];
			sprintf (libname, "d3dcompiler_%i.dll", i);

			if ((hInstCompiler = LoadLibrary (libname)) != NULL)
			{
				if ((QD3DCompile = (pD3DCompile) GetProcAddress (hInstCompiler, "D3DCompile")) != NULL)
					return;
				else
				{
					FreeLibrary (hInstCompiler);
					hInstCompiler = NULL;
				}
			}
		}
	}

	// error conditions
	if (!hInstCompiler) ri.Sys_Error (ERR_FATAL, "D_LoadShaderCompiler : LoadLibrary for d3dcompiler.dll failed");
	if (!QD3DCompile) ri.Sys_Error (ERR_FATAL, "D_LoadShaderCompiler : GetProcAddress for D3DCompile failed");
}


static HRESULT D_CompileShader (LPCVOID pSrcData, SIZE_T SrcDataSize, CONST D3D_SHADER_MACRO *pDefines, LPCSTR pEntrypoint, LPCSTR pTarget, ID3DBlob **ppCode)
{
	// and compile it
	ID3DBlob *errorBlob = NULL;
	HRESULT hr = QD3DCompile (pSrcData, SrcDataSize, NULL, pDefines, NULL, pEntrypoint, pTarget, 0, 0, ppCode, &errorBlob);

	// even if the compile succeeded there may be warnings so report everything here
	if (errorBlob)
	{
		char *compileError = (char *) errorBlob->lpVtbl->GetBufferPointer (errorBlob);
		ri.Con_Printf (PRINT_ALL, "Error compiling %s:\n%s\n", pEntrypoint, compileError);
		errorBlob->lpVtbl->Release (errorBlob);
	}

	return hr;
}


static char *D_LoadShaderSource (int resourceID)
{
	// we can't use ID3DInclude in C (we probably can but it's writing our own COM crap in C and life is not short enough for that)
	// so this is how we'll handle includes
	char *includesource = NULL;
	char *shadersource = NULL;

	// load the individual resources
	int includelength = LoadResourceData (IDR_COMMON, (void **) &includesource);
	int shaderlength = LoadResourceData (resourceID, (void **) &shadersource);

	// alloc sufficient memory for both
	char *fullsource = (char *) ri.Load_AllocMemory (includelength + shaderlength + 2);

	// and combine them
	strcpy (fullsource, includesource);
	strcpy (&fullsource[includelength], shadersource);

	// ensure this happens because resource-loading of an RCDATA resource will not necessarily terminate strings
	fullsource[includelength + shaderlength] = 0;

	return fullsource;
}


int D_CreateShaderBundle (int resourceID, const char *vsentry, const char *gsentry, const char *psentry, D3D11_INPUT_ELEMENT_DESC *layout, int numlayout)
{
	shaderbundle_t *sb = &d3d_Shaders[d3d_NumShaders];

	// set up to create
	char *shadersource = D_LoadShaderSource (resourceID);
	int shaderlength = strlen (shadersource);

	// explicitly NULL everything so that we can safely destroy stuff if anything fails
	memset (sb, 0, sizeof (*sb));

	// failed to load the resource
	if (!shaderlength || !shadersource) return -1;

	// load the compiler (or ensure it's loaded
	D_LoadShaderCompiler ();

	// make the vertex shader
	if (vsentry)
	{
		const D3D_SHADER_MACRO vsdefines[] = {
			{"VERTEXSHADER", "1"},
			{NULL, NULL}
		};

		ID3DBlob *vsBlob = NULL;

		if (SUCCEEDED (D_CompileShader (shadersource, shaderlength, vsdefines, vsentry, "vs_4_0", &vsBlob)))
		{
			if (vsBlob)
			{
				d3d_Device->lpVtbl->CreateVertexShader (d3d_Device, (DWORD *) vsBlob->lpVtbl->GetBufferPointer (vsBlob), vsBlob->lpVtbl->GetBufferSize (vsBlob), NULL, &sb->VertexShader);
				d3d_Device->lpVtbl->CreateInputLayout (d3d_Device, layout, numlayout, vsBlob->lpVtbl->GetBufferPointer (vsBlob), vsBlob->lpVtbl->GetBufferSize (vsBlob), &sb->InputLayout);
				vsBlob->lpVtbl->Release (vsBlob);
			}
		}
	}

	// make the geometry shader
	if (gsentry)
	{
		const D3D_SHADER_MACRO gsdefines[] = {
			{"GEOMETRYSHADER", "1"},
			{NULL, NULL}
		};

		ID3DBlob *gsBlob = NULL;

		if (SUCCEEDED (D_CompileShader (shadersource, shaderlength, gsdefines, gsentry, "gs_4_0", &gsBlob)))
		{
			if (gsBlob)
			{
				d3d_Device->lpVtbl->CreateGeometryShader (d3d_Device, (DWORD *) gsBlob->lpVtbl->GetBufferPointer (gsBlob), gsBlob->lpVtbl->GetBufferSize (gsBlob), NULL, &sb->GeometryShader);
				gsBlob->lpVtbl->Release (gsBlob);
			}
		}
	}

	// make the pixel shader
	if (psentry)
	{
		const D3D_SHADER_MACRO psdefines[] = {
			{"PIXELSHADER", "1"},
			{NULL, NULL}
		};

		ID3DBlob *psBlob = NULL;

		if (SUCCEEDED (D_CompileShader (shadersource, shaderlength, psdefines, psentry, "ps_4_0", &psBlob)))
		{
			if (psBlob)
			{
				d3d_Device->lpVtbl->CreatePixelShader (d3d_Device, (DWORD *) psBlob->lpVtbl->GetBufferPointer (psBlob), psBlob->lpVtbl->GetBufferSize (psBlob), NULL, &sb->PixelShader);
				psBlob->lpVtbl->Release (psBlob);
			}
		}
	}

	// throw away memory
	ri.Load_FreeMemory ();

	d3d_NumShaders++;

	// this is the actual shader id that was loaded
	return (d3d_NumShaders - 1);
}


void D_BindShaderBundle (int sb)
{
	D_SetInputLayout (d3d_Shaders[sb].InputLayout);
	D_SetVertexShader (d3d_Shaders[sb].VertexShader);
	D_SetGeometryShader (d3d_Shaders[sb].GeometryShader);
	D_SetPixelShader (d3d_Shaders[sb].PixelShader);
}


/*
============================================================================================================

CBUFFER MANAGEMENT

============================================================================================================
*/

void D_RegisterConstantBuffer (ID3D11Buffer *cBuffer, int slot)
{
	if (slot >= 0 && slot < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
		d3d_ConstantBuffers[slot] = cBuffer;
}


void D_BindConstantBuffers (void)
{
	// using D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT for compat with feature levels
	d3d_Context->lpVtbl->VSSetConstantBuffers (d3d_Context, 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, d3d_ConstantBuffers);
	d3d_Context->lpVtbl->GSSetConstantBuffers (d3d_Context, 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, d3d_ConstantBuffers);
	d3d_Context->lpVtbl->PSSetConstantBuffers (d3d_Context, 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, d3d_ConstantBuffers);
}


