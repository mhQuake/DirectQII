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
// gl_warp.c -- sky and water polygons

#include "r_local.h"

char	skyname[MAX_QPATH];
float	skyrotate;
vec3_t	skyaxis;

texture_t r_SkyCubemap;
texture_t r_WarpNoise;
rendertarget_t r_WaterWarpRT;

static int d3d_SurfDrawSkyShader;
static int d3d_WaterWarpShader;
static int d3d_SkyNoSkyShader;

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


void R_InitWarp (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0)
	};

	d3d_SurfDrawSkyShader = D_CreateShaderBundle (IDR_SURFSHADER, "SurfDrawSkyVS", NULL, "SurfDrawSkyPS", DEFINE_LAYOUT (layout));
	d3d_WaterWarpShader = D_CreateShaderBundle (IDR_WATERWARP, "WaterWarpVS", NULL, "WaterWarpPS", NULL, 0);
	d3d_SkyNoSkyShader = D_CreateShaderBundle (IDR_SURFSHADER, "SurfDrawSkyVS", NULL, "SkyNoSkyPS", DEFINE_LAYOUT (layout));

	R_CreateRenderTarget (&r_WaterWarpRT);
	D_CreateNoiseTexture ();
}


void R_ShutdownWarp (void)
{
	R_ReleaseTexture (&r_SkyCubemap);
	R_ReleaseTexture (&r_WarpNoise);
	R_ReleaseRenderTarget (&r_WaterWarpRT);
}


void R_SetupSky (QMATRIX *SkyMatrix)
{
	if (r_SkyCubemap.SRV)
	{
		// this is z-going-up with rotate and flip to orient the sky correctly
		R_MatrixLoadf (SkyMatrix, 0, 0, 1, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1);

		// rotate and position the sky
		R_MatrixRotateAxis (SkyMatrix, r_newrefdef.time * skyrotate, skyaxis[0], skyaxis[2], skyaxis[1]);
		R_MatrixTranslate (SkyMatrix, -r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

		// sky goes to slot 4
		d3d_Context->lpVtbl->PSSetShaderResources (d3d_Context, 4, 1, &r_SkyCubemap.SRV);
	}
	else R_MatrixIdentity (SkyMatrix);
}


void R_DrawSkyChain (msurface_t *surf)
{
	D_SetRenderStates (d3d_BSNone, d3d_DSDepthNoWrite, d3d_RSFullCull);

	if (r_lightmap->value)
		D_BindShaderBundle (d3d_SkyNoSkyShader);
	else D_BindShaderBundle (d3d_SurfDrawSkyShader);

	for (; surf; surf = surf->texturechain)
		R_AddSurfaceToBatch (surf);

	R_EndSurfaceBatch ();
}


/*
============
R_SetSky
============
*/
void R_SetSky (char *name, float rotate, vec3_t axis)
{
	int		i;

	byte	*sky_pic[6];
	int		sky_width[6];
	int		sky_height[6];

	// 3dstudio environment map names
	char	*suf[6] = {"ft", "bk", "up", "dn", "rt", "lf"};

	// maximum dimension of skyface images
	int		max_size = -1;

	D3D11_SUBRESOURCE_DATA srd[6];

	// shutdown the old sky
	R_ReleaseTexture (&r_SkyCubemap);

	// begin a new sky
	strncpy (skyname, name, sizeof (skyname) - 1);
	skyrotate = rotate;
	Vector3Copy (skyaxis, axis);

	for (i = 0; i < 6; i++)
	{
		// clear it down
		sky_pic[i] = NULL;
		sky_width[i] = sky_height[i] = -1;

		// attempt to load it
		LoadTGA (va ("env/%s%s.tga", skyname, suf[i]), &sky_pic[i], &sky_width[i], &sky_height[i]);

		// figure the max size to create the cubemap at
		if (sky_width[i] > max_size) max_size = sky_width[i];
		if (sky_height[i] > max_size) max_size = sky_height[i];
	}

	// only proceed if we got something
	if (max_size < 1) return;

	// now set up the skybox faces for the cubemap
	for (i = 0; i < 6; i++)
	{
		if (sky_width[i] != max_size || sky_height[i] != max_size)
		{
			// if the size is different (e.g. a smaller bottom face may be provided due to some misguided attempt to "save memory")
			// we need to resample it up to the correct size because of cubemap creation constraints
			if (sky_pic[i])
				sky_pic[i] = (byte *) Image_ResampleToSize ((unsigned *) sky_pic[i], sky_width[i], sky_height[i], max_size, max_size);
			else
			{
				// case where a sky face may be omitted due to some misguided attempt to "save memory"
				sky_pic[i] = (byte *) ri.Load_AllocMemory (max_size * max_size * 4);
				memset (sky_pic[i], 0, max_size * max_size * 4);
			}

			// and set the new data
			sky_width[i] = max_size;
			sky_height[i] = max_size;
		}

		// and copy them off to the SRD
		srd[i].pSysMem = sky_pic[i];
		srd[i].SysMemPitch = max_size << 2;
		srd[i].SysMemSlicePitch = 0;
	}

	// and create it
	R_CreateTexture (&r_SkyCubemap, srd, max_size, max_size, 1, TEX_RGBA8 | TEX_CUBEMAP);

	// throw away memory used for loading
	ri.Load_FreeMemory ();
}


qboolean D_BeginWaterWarp (void)
{
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return false;

	if (r_newrefdef.rdflags & RDF_UNDERWATER)
	{
		d3d_Context->lpVtbl->OMSetRenderTargets (d3d_Context, 1, &r_WaterWarpRT.RTV, d3d_DepthBuffer);
		R_Clear (r_WaterWarpRT.RTV, d3d_DepthBuffer);
		return true;
	}

	// normal, unwarped scene
	R_Clear (d3d_RenderTarget, d3d_DepthBuffer);
	return false;
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

