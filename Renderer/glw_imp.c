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
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
**
*/
#include <assert.h>
#include <windows.h>
#include "r_local.h"
#include "../DirectQII/winquake.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

// this just needs to be included anywhere....
#pragma comment (lib, "dxguid.lib")

/*
=========================================================================================================================================================================

MAIN OBJECTS

=========================================================================================================================================================================
*/

// main d3d objects
ID3D11Device *d3d_Device = NULL;
ID3D11DeviceContext *d3d_Context = NULL;
IDXGISwapChain *d3d_SwapChain = NULL;

ID3D11RenderTargetView *d3d_RenderTarget = NULL;
ID3D11DepthStencilView *d3d_DepthBuffer = NULL;



/*
=========================================================================================================================================================================

OBJECT LIFETIME MANAGEMENT

=========================================================================================================================================================================
*/

// object cache lets us store out objects on creation so that indivdual routines don't need to destroy them
#define MAX_OBJECT_CACHE	1024

typedef struct d3dobject_s {
	ID3D11DeviceChild *Object;
	char *name;
} d3dobject_t;

d3dobject_t ObjectCache[MAX_OBJECT_CACHE];
int NumObjectCache = 0;

void D_CacheObject (ID3D11DeviceChild *Object, const char *name)
{
	if (!Object)
		return;
	else if (NumObjectCache < MAX_OBJECT_CACHE)
	{
		ObjectCache[NumObjectCache].Object = Object;
		ObjectCache[NumObjectCache].name = (char *) ri.Zone_Alloc (strlen (name) + 1);
		strcpy (ObjectCache[NumObjectCache].name, name);

		NumObjectCache++;
	}
	else Sys_Error ("R_CacheObject : object cache overflow!");
}


void D_ReleaseObjectCache (void)
{
	int i;

	for (i = 0; i < MAX_OBJECT_CACHE; i++)
		SAFE_RELEASE (ObjectCache[i].Object);

	memset (ObjectCache, 0, sizeof (ObjectCache));
}


/*
=========================================================================================================================================================================

STOCK CODE CONTINUES

=========================================================================================================================================================================
*/

qboolean GLimp_InitGL (void);

typedef struct glwstate_s
{
	HINSTANCE	hInstance;
	void *wndproc;

	HWND    hWnd;			// handle to window
	qboolean fullscreen;
} glwstate_t;

glwstate_t glw_state;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_ref;

static qboolean VerifyDriver (void)
{
	return true;
}

void VID_ScaleVidDef (viddef_t *vd, int w, int h)
{
	vd->width = w;
	vd->height = h;

#if 1
	vd->conwidth = w;
	vd->conheight = h;
#else
#define VIRTUAL_HEIGHT 600
	if (h > VIRTUAL_HEIGHT)
	{
		vd->conwidth = (VIRTUAL_HEIGHT * w) / h;
		vd->conheight = VIRTUAL_HEIGHT;
	}
	else
	{
		vd->conwidth = w;
		vd->conheight = h;
	}
#endif

	ri.Vid_NewWindow (vd);
}


/*
** VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME	"DirectQII"

qboolean VID_CreateWindow (int width, int height, qboolean fullscreen)
{
	WNDCLASS		wc;
	RECT			r;
	cvar_t			*vid_xpos, *vid_ypos;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	/* Register the frame class */
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC) glw_state.wndproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = glw_state.hInstance;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (void *) COLOR_GRAYTEXT;
	wc.lpszMenuName = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass (&wc))
		ri.Sys_Error (ERR_FATAL, "Couldn't register window class");

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP | WS_VISIBLE;
	}
	else
	{
		exstyle = 0;
		stylebits = WINDOW_STYLE;
	}

	r.left = 0;
	r.top = 0;
	r.right = width;
	r.bottom = height;

	AdjustWindowRect (&r, stylebits, FALSE);

	w = r.right - r.left;
	h = r.bottom - r.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else
	{
		vid_xpos = ri.Cvar_Get ("vid_xpos", "0", 0);
		vid_ypos = ri.Cvar_Get ("vid_ypos", "0", 0);
		x = vid_xpos->value;
		y = vid_ypos->value;
	}

	glw_state.hWnd = CreateWindowEx (
		exstyle,
		WINDOW_CLASS_NAME,
		WINDOW_CLASS_NAME,
		stylebits,
		x, y, w, h,
		NULL,
		NULL,
		glw_state.hInstance,
		NULL);

	if (!glw_state.hWnd)
		ri.Sys_Error (ERR_FATAL, "Couldn't create window");

	ShowWindow (glw_state.hWnd, SW_SHOW);
	UpdateWindow (glw_state.hWnd);

	// init all the gl stuff for the window
	if (!GLimp_InitGL ())
	{
		ri.Con_Printf (PRINT_ALL, "VID_CreateWindow() - GLimp_InitGL failed\n");
		return false;
	}

	SetForegroundWindow (glw_state.hWnd);
	SetFocus (glw_state.hWnd);

	// let the sound and input subsystems know about the new window
	VID_ScaleVidDef (&vid, width, height);

	return true;
}


/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode (int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	int width, height;
	const char *win_fs[] = {"W", "FS"};

	ri.Con_Printf (PRINT_ALL, "Initializing OpenGL display\n");
	ri.Con_Printf (PRINT_ALL, "...setting mode %d:", mode);

	if (!ri.Vid_GetModeInfo (&width, &height, mode))
	{
		ri.Con_Printf (PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	ri.Con_Printf (PRINT_ALL, " %d %d %s\n", width, height, win_fs[fullscreen]);

	// destroy the existing window
	if (glw_state.hWnd)
	{
		GLimp_Shutdown ();
	}

	// do a CDS if needed
	if (fullscreen)
	{
		ri.Con_Printf (PRINT_ALL, "...setting fullscreen mode\n");

		*pwidth = width;
		*pheight = height;
		glw_state.fullscreen = true;

		// if we fail to create a fullscreen mode call recursively to create a windowed mode
		if (!VID_CreateWindow (width, height, true))
			return GLimp_SetMode (pwidth, pheight, mode, false);
	}
	else
	{
		ri.Con_Printf (PRINT_ALL, "...setting windowed mode\n");

		*pwidth = width;
		*pheight = height;
		glw_state.fullscreen = false;

		if (!VID_CreateWindow (width, height, false))
			return rserr_invalid_mode;
	}

	return rserr_ok;
}


void GLimp_ClearToBlack (void)
{
	float clearColor[] = {0, 0, 0, 0};

	// clear to black
	d3d_Context->lpVtbl->ClearRenderTargetView (d3d_Context, d3d_RenderTarget, clearColor);

	// and run a present to make them show
	d3d_Context->lpVtbl->OMSetRenderTargets (d3d_Context, 1, &d3d_RenderTarget, d3d_DepthBuffer);
	d3d_SwapChain->lpVtbl->Present (d3d_SwapChain, 0, 0);

	// ensure that the window paints
	ri.SendKeyEvents ();
	Sleep (5);
}


/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown (void)
{
	if (d3d_Device && d3d_SwapChain && d3d_Context)
	{
		if (glw_state.fullscreen)
		{
			// DXGI prohibits destruction of a swapchain when in a fullscreen mode so go back to windowed first
			d3d_SwapChain->lpVtbl->SetFullscreenState (d3d_SwapChain, FALSE, NULL);
			glw_state.fullscreen = false;
		}

		GLimp_ClearToBlack ();

		// finally chuck out all cached state in the context
		d3d_Context->lpVtbl->OMSetRenderTargets (d3d_Context, 0, NULL, NULL);
		d3d_Context->lpVtbl->ClearState (d3d_Context);
		d3d_Context->lpVtbl->Flush (d3d_Context);
	}

	// handle special objects
	R_ShutdownShaders ();
	R_ShutdownSurfaces ();
	R_ShutdownLight ();
	R_ShutdownWarp ();
	R_ShutdownMesh ();
	R_ShutdownBeam ();

	// handle cacheable objects
	D_ReleaseObjectCache ();

	// destroy main objects
	SAFE_RELEASE (d3d_DepthBuffer);
	SAFE_RELEASE (d3d_RenderTarget);

	SAFE_RELEASE (d3d_SwapChain);
	SAFE_RELEASE (d3d_Context);
	SAFE_RELEASE (d3d_Device);

	if (glw_state.hWnd)
	{
		DestroyWindow (glw_state.hWnd);
		glw_state.hWnd = NULL;
	}

	UnregisterClass (WINDOW_CLASS_NAME, glw_state.hInstance);
}


/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
qboolean GLimp_Init (void *hinstance, void *wndproc)
{
	glw_state.hInstance = (HINSTANCE) hinstance;
	glw_state.wndproc = wndproc;

	return true;
}


void GLimp_CreateFrameBuffer (void)
{
	ID3D11Texture2D *pBackBuffer = NULL;
	D3D11_TEXTURE2D_DESC descRT;

	ID3D11Texture2D *pDepthStencil = NULL;
	D3D11_TEXTURE2D_DESC descDepth;

	// Get a pointer to the back buffer
	if (FAILED (d3d_SwapChain->lpVtbl->GetBuffer (d3d_SwapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer)))
	{
		ri.Sys_Error (ERR_FATAL, "d3d_SwapChain->GetBuffer failed");
		return;
	}

	// get the description of the backbuffer for creating the depth buffer from it
	pBackBuffer->lpVtbl->GetDesc (pBackBuffer, &descRT);

	// Create a render-target view
	d3d_Device->lpVtbl->CreateRenderTargetView (d3d_Device, (ID3D11Resource *) pBackBuffer, NULL, &d3d_RenderTarget);
	pBackBuffer->lpVtbl->Release (pBackBuffer);

	// create the depth buffer with the same dimensions and multisample levels as the backbuffer
	descDepth.Width = descRT.Width;
	descDepth.Height = descRT.Height;
	descDepth.SampleDesc.Count = descRT.SampleDesc.Count;
	descDepth.SampleDesc.Quality = descRT.SampleDesc.Quality;

	// fill in the rest of it
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	// and create it
	if (SUCCEEDED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &descDepth, NULL, &pDepthStencil)))
	{
		// Create the depth stencil view
		d3d_Device->lpVtbl->CreateDepthStencilView (d3d_Device, (ID3D11Resource *) pDepthStencil, NULL, &d3d_DepthBuffer);
		pDepthStencil->lpVtbl->Release (pDepthStencil);
	}

	GLimp_ClearToBlack ();
}


qboolean GLimp_InitGL (void)
{
	RECT cr;
	DXGI_SWAP_CHAIN_DESC sd;
	IDXGIFactory *pFactory = NULL;

	// we don't support any d3d9 feature levels
	D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

	memset (&sd, 0, sizeof (sd));
	// to do...
	// memcpy (&sd.BufferDesc, &d3d_VideoModes[modenum], sizeof (DXGI_MODE_DESC));

	GetClientRect (glw_state.hWnd, &cr);

	sd.BufferDesc.Width = cr.right - cr.left;
	sd.BufferDesc.Height = cr.bottom - cr.top;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 0;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

	sd.BufferCount = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = glw_state.hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// always initially create a windowed swapchain at the desired resolution, then switch to fullscreen post-creation if it was requested
	sd.Windowed = TRUE;

	// now we try to create it
	if (FAILED (D3D11CreateDeviceAndSwapChain (
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0, //D3D11_CREATE_DEVICE_DEBUG,
		FeatureLevels,
		sizeof (FeatureLevels) / sizeof (FeatureLevels[0]),
		D3D11_SDK_VERSION,
		&sd,
		&d3d_SwapChain,
		&d3d_Device,
		NULL,
		&d3d_Context)))
	{
		ri.Sys_Error (ERR_FATAL, "D3D11CreateDeviceAndSwapChain failed");
		return false;
	}

	// now we disable stuff that we want to handle ourselves
	if (SUCCEEDED (d3d_SwapChain->lpVtbl->GetParent (d3d_SwapChain, &IID_IDXGIFactory, (void **) &pFactory)))
	{
		pFactory->lpVtbl->MakeWindowAssociation (pFactory, glw_state.hWnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
		pFactory->lpVtbl->Release (pFactory);
	}

	// now we switch to fullscreen after the device, context and swapchain are created, but before we create the rendertargets, so that we don't need to respond to WM_SIZE
	if (glw_state.fullscreen)
		d3d_SwapChain->lpVtbl->SetFullscreenState (d3d_SwapChain, TRUE, NULL);

	// create the initial frame buffer that we're going to use for all of our rendering
	GLimp_CreateFrameBuffer ();

	// load all of our initial objects
	// each subsystem creates it's objects then registers it's handlers, following which the reset handler runs to complete object creation
	R_InitQuadBatch ();
	R_InitMain ();
	R_InitSurfaces ();
	R_InitParticles ();
	R_InitSprites ();
	R_InitLight ();
	R_InitWarp ();
	R_InitMesh ();
	R_InitBeam ();
	R_InitNull ();

	// success
	return true;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame (void)
{
	// go into 2D mode (this just sets a viewport)
	R_Set2DMode ();

	// set up the 2D ortho view, brightness and contrast
	D_UpdateDrawConstants (vid.conwidth, vid.conheight, vid_gamma->value, 1.0f);

	// everything in all draws is drawn as an indexed triangle list, even if it's ultimately a strip or a single tri, so this can be set-and-forget once per frame
	d3d_Context->lpVtbl->IASetPrimitiveTopology (d3d_Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// bind buffers and samplers that will remain bound for the duration of the frame
	D_BindSamplers ();
	D_BindConstantBuffers ();
}


/*
** GLimp_EndFrame
**
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (int syncinterval)
{
	// free any loading memory that may have been used during the frame
	ri.Load_FreeMemory ();
	d3d_SwapChain->lpVtbl->Present (d3d_SwapChain, syncinterval, 0);
}


/*
** GLimp_AppActivate
*/
void GLimp_AppActivate (qboolean active)
{
	if (active)
	{
		SetForegroundWindow (glw_state.hWnd);
		ShowWindow (glw_state.hWnd, SW_RESTORE);
	}
	else
	{
		if (vid_fullscreen->value)
			ShowWindow (glw_state.hWnd, SW_MINIMIZE);
	}
}
