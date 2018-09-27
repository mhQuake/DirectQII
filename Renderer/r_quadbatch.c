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

// draw.c

#include "r_local.h"


typedef struct quadpolyvert_s {
	float position[3];
	union {
		DWORD color;
		byte rgba[4];
	};
	float texcoord[2];
} quadpolyvert_t;

typedef struct quadpolyvert_texarray_s {
	float position[3];
	float texcoord[3];
} quadpolyvert_texarray_t;

// these should be sized such that MAX_DRAW_INDEXES = (MAX_DRAW_VERTS / 4) * 6
// sized larger for particles going through this path
#define MAX_QUAD_VERTS		0x4000
#define MAX_QUAD_INDEXES	0x6000

#if (MAX_QUAD_VERTS / 4) * 6 != MAX_QUAD_INDEXES
#error (MAX_QUAD_VERTS / 4) * 6 != MAX_QUAD_INDEXES
#endif


static quadpolyvert_t *d_quadverts = NULL;
static int d_firstquadvert = 0;
static int d_numquadverts = 0;

static ID3D11Buffer *d3d_QuadVertexes = NULL;
static ID3D11Buffer *d3d_QuadIndexes = NULL;


void R_InitQuadBatch (void)
{
	D3D11_BUFFER_DESC vbDesc = {
		sizeof (quadpolyvert_t) * MAX_QUAD_VERTS,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_VERTEX_BUFFER,
		D3D11_CPU_ACCESS_WRITE,
		0,
		0
	};

	D3D11_BUFFER_DESC ibDesc = {
		sizeof (unsigned short) * MAX_QUAD_INDEXES,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	int i;
	unsigned short *ndx = ri.Load_AllocMemory (sizeof (unsigned short) * MAX_QUAD_INDEXES);
	D3D11_SUBRESOURCE_DATA srd = {ndx, 0, 0};

	for (i = 0; i < MAX_QUAD_VERTS; i += 4, ndx += 6)
	{
		ndx[0] = i + 0;
		ndx[1] = i + 1;
		ndx[2] = i + 2;

		ndx[3] = i + 0;
		ndx[4] = i + 2;
		ndx[5] = i + 3;
	}

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, NULL, &d3d_QuadVertexes);
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &d3d_QuadIndexes);

	ri.Load_FreeMemory ();

	D_CacheObject ((ID3D11DeviceChild *) d3d_QuadIndexes, "d3d_QuadIndexes");
	D_CacheObject ((ID3D11DeviceChild *) d3d_QuadVertexes, "d3d_QuadVertexes");
}


int D_CreateShaderBundleForQuadBatch (int resourceID, const char *vsentry, const char *psentry, batchtype_t type)
{
	D3D11_INPUT_ELEMENT_DESC layout_standard[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		VDECL ("COLOUR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0)
	};

	D3D11_INPUT_ELEMENT_DESC layout_texarray[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0)
	};

	switch (type)
	{
	case batch_standard: return D_CreateShaderBundle (resourceID, vsentry, NULL, psentry, DEFINE_LAYOUT (layout_standard));
	case batch_texarray: return D_CreateShaderBundle (resourceID, vsentry, NULL, psentry, DEFINE_LAYOUT (layout_texarray));
	default: ri.Sys_Error (ERR_FATAL, "D_CreateShaderBundleForQuadBatch : bad batchtype");
	}

	// never reached; shut up compiler
	return -1;
}


void D_QuadVertexPosition2fColorTexCoord2f (float x, float y, DWORD color, float s, float t)
{
	d_quadverts[d_numquadverts].texcoord[0] = s;
	d_quadverts[d_numquadverts].texcoord[1] = t;

	d_quadverts[d_numquadverts].color = color;

	d_quadverts[d_numquadverts].position[0] = x;
	d_quadverts[d_numquadverts].position[1] = y;
	d_quadverts[d_numquadverts].position[2] = 0;

	d_numquadverts++;
}


void D_QuadVertexPosition2fTexCoord2f (float x, float y, float s, float t)
{
	d_quadverts[d_numquadverts].texcoord[0] = s;
	d_quadverts[d_numquadverts].texcoord[1] = t;

	d_quadverts[d_numquadverts].color = 0xffffffff;

	d_quadverts[d_numquadverts].position[0] = x;
	d_quadverts[d_numquadverts].position[1] = y;
	d_quadverts[d_numquadverts].position[2] = 0;

	d_numquadverts++;
}


void D_QuadVertexPosition2fColor (float x, float y, DWORD color)
{
	d_quadverts[d_numquadverts].color = color;

	d_quadverts[d_numquadverts].position[0] = x;
	d_quadverts[d_numquadverts].position[1] = y;
	d_quadverts[d_numquadverts].position[2] = 0;

	d_numquadverts++;
}


void D_QuadVertexPosition2fTexCoord3f (float x, float y, float s, float t, float slice)
{
	quadpolyvert_texarray_t *vert = (quadpolyvert_texarray_t *) &d_quadverts[d_numquadverts];

	vert->position[0] = x;
	vert->position[1] = y;
	vert->position[2] = 0;

	vert->texcoord[0] = s;
	vert->texcoord[1] = t;
	vert->texcoord[2] = slice;

	d_numquadverts++;
}


void D_CheckQuadBatch (void)
{
	if (d_firstquadvert + d_numquadverts + 4 >= MAX_QUAD_VERTS)
	{
		// if we run out of buffer space for the next quad we flush the batch and begin a new one
		D_EndQuadBatch ();
		d_firstquadvert = 0;
	}

	if (!d_quadverts)
	{
		// first index is only reset to 0 if the buffer must wrap so this is valid to do
		D3D11_MAP mode = (d_firstquadvert > 0) ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
		D3D11_MAPPED_SUBRESOURCE msr;

		if (FAILED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) d3d_QuadVertexes, 0, mode, 0, &msr)))
			return;
		else d_quadverts = (quadpolyvert_t *) msr.pData + d_firstquadvert;
	}
}


void D_EndQuadBatch (void)
{
	if (d_quadverts)
	{
		d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) d3d_QuadVertexes, 0);
		d_quadverts = NULL;
	}

	if (d_numquadverts)
	{
		D_BindVertexBuffer (0, d3d_QuadVertexes, sizeof (quadpolyvert_t), 0);
		D_BindIndexBuffer (d3d_QuadIndexes, DXGI_FORMAT_R16_UINT);

		d3d_Context->lpVtbl->DrawIndexed (d3d_Context, (d_numquadverts >> 2) * 6, 0, d_firstquadvert);

		d_firstquadvert += d_numquadverts;
		d_numquadverts = 0;
	}
}


