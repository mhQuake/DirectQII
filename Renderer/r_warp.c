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


static int d3d_WaterWarpShader;
texture_t r_WarpNoise;
rendertarget_t r_WaterWarpRT;


void D_CreateNoiseTexture (void)
{
#define NOISESIZE	16
	int x, y;
	unsigned *data = ri.Load_AllocMemory (NOISESIZE * NOISESIZE * 4);
	unsigned *dst = data; // preserve the original pointer so that we can use it for an SRD
	D3D11_SUBRESOURCE_DATA srd;

	for (y = 0; y < NOISESIZE; y++)
	{
		short *vu = (short *) dst;

		for (x = 0; x < NOISESIZE; x++)
		{
			*vu++ = (rand () & 65535) - 32768;
			*vu++ = (rand () & 65535) - 32768;
		}

		dst += NOISESIZE;
	}

	// set up the SRD
	srd.pSysMem = data;
	srd.SysMemPitch = NOISESIZE << 2;
	srd.SysMemSlicePitch = 0;

	// and create it
	R_CreateTexture (&r_WarpNoise, &srd, NOISESIZE, NOISESIZE, 1, TEX_R16G16);
	ri.Load_FreeMemory ();
}


qboolean D_BeginWaterWarp (void)
{
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return false;

	if (r_newrefdef.rdflags & RDF_UNDERWATER)
	{
		// underwater warped
		d3d_Context->lpVtbl->OMSetRenderTargets (d3d_Context, 1, &r_WaterWarpRT.RTV, d3d_DepthBuffer);
		R_Clear (r_WaterWarpRT.RTV, d3d_DepthBuffer);
		return true;
	}
	else
	{
		// normal, unwarped scene
		R_Clear (d3d_RenderTarget, d3d_DepthBuffer);
		return false;
	}
}


void D_DoWaterWarp (void)
{
	// revert the original RTs
	d3d_Context->lpVtbl->OMSetRenderTargets (d3d_Context, 1, &d3d_RenderTarget, d3d_DepthBuffer);

	// noise goes to slot 5
	d3d_Context->lpVtbl->PSSetShaderResources (d3d_Context, 5, 1, &r_WarpNoise.SRV);

	// and draw it
	D_BindShaderBundle (d3d_WaterWarpShader);
	D_SetRenderStates (d3d_BSNone, d3d_DSNoDepth, d3d_RSNoCull);

	// this can't be bound once at the start of the frame because setting it as an RT will unbind it
	R_BindTexture (r_WaterWarpRT.SRV);

	// full-screen triangle
	d3d_Context->lpVtbl->Draw (d3d_Context, 3, 0);
}


void R_InitWarp (void)
{
	d3d_WaterWarpShader = D_CreateShaderBundle (IDR_WATERWARP, "WaterWarpVS", NULL, "WaterWarpPS", NULL, 0);

	R_CreateRenderTarget (&r_WaterWarpRT);
	D_CreateNoiseTexture ();
}


void R_ShutdownWarp (void)
{
	R_ReleaseTexture (&r_WarpNoise);
	R_ReleaseRenderTarget (&r_WaterWarpRT);
}



