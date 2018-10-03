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
// r_misc.c

#include "r_local.h"

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// special texture loading
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
void R_CreateSpecialTextures (void)
{
	unsigned blacktexturedata[4] = {0xff000000, 0xff000000, 0xff000000, 0xff000000};
	unsigned greytexturedata[4] = {0xff7f7f7f, 0xff7f7f7f, 0xff7f7f7f, 0xff7f7f7f};
	unsigned whitetexturedata[4] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
	unsigned notexturedata[4] = {0xff000000, 0xff7f7f7f, 0xff7f7f7f, 0xffffffff};

	r_blacktexture = GL_LoadPic ("***r_blacktexture***", (byte *) blacktexturedata, 2, 2, it_wall, 32, NULL);
	r_greytexture = GL_LoadPic ("***r_greytexture***", (byte *) greytexturedata, 2, 2, it_wall, 32, NULL);
	r_whitetexture = GL_LoadPic ("***r_whitetexture***", (byte *) whitetexturedata, 2, 2, it_wall, 32, NULL);
	r_notexture = GL_LoadPic ("***r_notexture***", (byte *) notexturedata, 2, 2, it_wall, 32, NULL);
}


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// screenshots
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
void D_CaptureScreenshot (char *checkname)
{
	ID3D11Texture2D *pBackBuffer = NULL;
	ID3D11Texture2D *pScreenShot = NULL;
	D3D11_TEXTURE2D_DESC descRT;
	D3D11_MAPPED_SUBRESOURCE msr;

	// Get a pointer to the back buffer
	if (FAILED (d3d_SwapChain->lpVtbl->GetBuffer (d3d_SwapChain, 0, &IID_ID3D11Texture2D, (LPVOID *) &pBackBuffer)))
	{
		ri.Con_Printf (PRINT_ALL, "d3d_SwapChain->GetBuffer failed");
		return;
	}

	// get the description of the backbuffer for creating the depth buffer from it
	pBackBuffer->lpVtbl->GetDesc (pBackBuffer, &descRT);

	// set the stuff we want to differ
	descRT.Usage = D3D11_USAGE_STAGING;
	descRT.BindFlags = 0;
	descRT.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

	// create a copy of the back buffer
	if (FAILED (d3d_Device->lpVtbl->CreateTexture2D (d3d_Device, &descRT, NULL, &pScreenShot)))
	{
		ri.Con_Printf (PRINT_ALL, "SCR_GetScreenData : failed to create scratch texture\n");
		goto failed;
	}

	// copy over the back buffer to the screenshot texture
	d3d_Context->lpVtbl->CopyResource (d3d_Context, (ID3D11Resource *) pScreenShot, (ID3D11Resource *) pBackBuffer);
	R_SyncPipeline ();

	if (FAILED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) pScreenShot, 0, D3D11_MAP_READ_WRITE, 0, &msr)))
	{
		ri.Con_Printf (PRINT_ALL, "SCR_GetScreenData : failed to map scratch texture\n");
		goto failed;
	}

	// now convert it down
	Image_CollapseRowPitch ((unsigned *) msr.pData, descRT.Width, descRT.Height, msr.RowPitch >> 2);
	Image_Compress32To24RGBtoBGR ((byte *) msr.pData, descRT.Width, descRT.Height);

	// and write it out
	Image_WriteDataToTGA (checkname, msr.pData, descRT.Width, descRT.Height, 24);

	// unmap, and...
	d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) pScreenShot, 0);

	// ...done
	ri.Con_Printf (PRINT_ALL, "Wrote %s\n", checkname);

failed:;
	// clean up
	SAFE_RELEASE (pScreenShot);
	SAFE_RELEASE (pBackBuffer);
}


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// beams
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct beampolyvert_s {
	// this exists so that i don't get confused over what the actual count of verts is
	// because "* 3" for number of floats in a vert vs "* 3" for number of verts in a triangle caused me to come a cropper with this code before
	float position[3];
} beampolyvert_t;

int r_numbeamverts = 0;
int r_numbeamindexes = 0;

ID3D11Buffer *d3d_BeamVertexes = NULL;
ID3D11Buffer *d3d_BeamIndexes = NULL;

void R_CreateBeamVertexBuffer (D3D11_SUBRESOURCE_DATA *vbSrd)
{
	D3D11_BUFFER_DESC vbDesc = {
		r_numbeamverts * sizeof (beampolyvert_t),
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, vbSrd, &d3d_BeamVertexes);
}


void R_CreateBeamIndexBuffer (void)
{
	D3D11_BUFFER_DESC ibDesc = {
		r_numbeamindexes * sizeof (unsigned short),
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	unsigned short *indexes = (unsigned short *) ri.Load_AllocMemory (r_numbeamindexes * sizeof (unsigned short));
	unsigned short *ndx = indexes;

	D3D11_SUBRESOURCE_DATA srd = {indexes, 0, 0};

	int i, numindexes = 0;

	for (i = 2; i < r_numbeamverts; i++, ndx += 3, numindexes += 3)
	{
		ndx[0] = i - 2;
		ndx[1] = (i & 1) ? i : (i - 1);
		ndx[2] = (i & 1) ? (i - 1) : i;
	}

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &d3d_BeamIndexes);
}


void R_CreateBeamVertexes (int slices)
{
	int i;
	beampolyvert_t *verts = NULL;
	D3D11_SUBRESOURCE_DATA srd;

	// clamp sensibly
	if (slices < 3) slices = 3;
	if (slices > 360) slices = 360;

	r_numbeamverts = (slices + 1) * 2;
	r_numbeamindexes = (r_numbeamverts - 2) * 3;
	verts = (beampolyvert_t *) ri.Load_AllocMemory (r_numbeamverts * sizeof (beampolyvert_t));

	srd.pSysMem = verts;
	srd.SysMemPitch = 0;
	srd.SysMemSlicePitch = 0;

	for (i = 0; i <= slices; i++, verts += 2)
	{
		float angle = (i == slices) ? 0.0f : (2.0f * M_PI * (float) i / (float) slices);

		Vector3Set (verts[0].position, sin (angle) * 0.5f, cos (angle) * 0.5f, 1);
		Vector3Set (verts[1].position, sin (angle) * 0.5f, cos (angle) * 0.5f, 0);
	}

	// create the buffers from the generated data
	R_CreateBeamVertexBuffer (&srd);
	R_CreateBeamIndexBuffer ();

	// throw away memory used for loading
	ri.Load_FreeMemory ();
}


static int d3d_BeamShader = 0;

void R_InitBeam (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0)
	};

	d3d_BeamShader = D_CreateShaderBundle (IDR_MISCSHADER, "BeamVS", NULL, "BeamPS", DEFINE_LAYOUT (layout));

	if (r_beamdetail)
	{
		R_CreateBeamVertexes (r_beamdetail->value);
		r_beamdetail->modified = false; // don't trigger unnecessarily
	}
	else R_CreateBeamVertexes (24);
}


void R_ShutdownBeam (void)
{
	SAFE_RELEASE (d3d_BeamVertexes);
	SAFE_RELEASE (d3d_BeamIndexes);
}


void R_DrawBeam (entity_t *e, QMATRIX *localmatrix)
{
	// don't draw fully translucent beams
	if (e->alpha > 0)
	{
		float color[3] = {
			(float) ((byte *) &d_8to24table[e->skinnum & 0xff])[0] / 255.0f,
			(float) ((byte *) &d_8to24table[e->skinnum & 0xff])[1] / 255.0f,
			(float) ((byte *) &d_8to24table[e->skinnum & 0xff])[2] / 255.0f
		};

		R_UpdateEntityConstants (localmatrix, color, 0);
		R_UpdateEntityAlphaState (RF_TRANSLUCENT, e->alpha);
		D_BindShaderBundle (d3d_BeamShader);

		if (r_beamdetail->modified)
		{
			R_ShutdownBeam ();
			R_CreateBeamVertexes (r_beamdetail->value);
			r_beamdetail->modified = false;
		}

		D_BindVertexBuffer (0, d3d_BeamVertexes, sizeof (beampolyvert_t), 0);
		D_BindIndexBuffer (d3d_BeamIndexes, DXGI_FORMAT_R16_UINT);

		d3d_Context->lpVtbl->DrawIndexed (d3d_Context, r_numbeamindexes, 0, 0);
	}
}


void R_PrepareBeam (entity_t *e, QMATRIX *localmatrix)
{
	float dir[3], len, axis[3], upvec[3] = {0, 0, 1};

	Vector3Subtract (dir, e->prevorigin, e->currorigin);
	Vector3Cross (axis, upvec, dir);

	// catch 0 length beams
	if (!((len = Vector3Length (dir)) > 0))
	{
		// hack to not draw it
		e->alpha = 0;
		return;
	}

	// and transform it (there really should be a better way of doing this)
	R_MatrixIdentity (localmatrix);
	R_MatrixTranslate (localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotateAxis (localmatrix, (180.0f / M_PI) * acos ((Vector3Dot (upvec, dir) / len)), axis[0], axis[1], axis[2]);
	R_MatrixScale (localmatrix, e->currframe, e->currframe, len);
}


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// null models
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
int d3d_NullShader = 0;

void R_InitNull (void)
{
	d3d_NullShader = D_CreateShaderBundle (IDR_MISCSHADER, "NullVS", "NullGS", "NullPS", NULL, 0);
}


void R_DrawNullModel (entity_t *e)
{
	float shadelight[3];
	QMATRIX localmatrix;

	if (e->flags & RF_FULLBRIGHT)
		Vector3Set (shadelight, 1, 1, 1);
	else
	{
		R_LightPoint (e->currorigin, shadelight);
		R_DynamicLightPoint (e->currorigin, shadelight);
	}

	R_MatrixIdentity (&localmatrix);
	R_MatrixTranslate (&localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotate (&localmatrix, -e->angles[0], e->angles[1], -e->angles[2]);

	R_UpdateEntityConstants (&localmatrix, shadelight, 0);
	R_UpdateEntityAlphaState (e->flags, e->alpha);
	D_BindShaderBundle (d3d_NullShader);

	d3d_Context->lpVtbl->Draw (d3d_Context, 24, 0);
}



// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// particles
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
static int d3d_ParticleCircleShader;
static int d3d_ParticleSquareShader;
static int d3d_ParticleShader = 0;

static ID3D11Buffer *d3d_ParticleVertexes = NULL;

static const int MAX_GPU_PARTICLES = (MAX_PARTICLES * 10);
static int r_FirstParticle = 0;

void R_InitParticles (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("ORIGIN", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		VDECL ("VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		VDECL ("ACCELERATION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		VDECL ("TIME", 0, DXGI_FORMAT_R32_FLOAT, 0, 0),
		VDECL ("COLOR", 0, DXGI_FORMAT_R32_SINT, 0, 0),
		VDECL ("ALPHA", 0, DXGI_FORMAT_R32_FLOAT, 0, 0)
	};

	D3D11_BUFFER_DESC vbDesc = {
		sizeof (particle_t) * MAX_GPU_PARTICLES,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_VERTEX_BUFFER,
		D3D11_CPU_ACCESS_WRITE,
		0,
		0
	};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, NULL, &d3d_ParticleVertexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_ParticleVertexes, "d3d_ParticleVertexes");

	// creating a square shader even though we don't currently use it
	d3d_ParticleCircleShader = D_CreateShaderBundle (IDR_MISCSHADER, "ParticleVS", "ParticleCircleGS", "ParticleCirclePS", DEFINE_LAYOUT (layout));
	d3d_ParticleSquareShader = D_CreateShaderBundle (IDR_MISCSHADER, "ParticleVS", "ParticleSquareGS", "ParticleSquarePS", DEFINE_LAYOUT (layout));
	d3d_ParticleShader = d3d_ParticleCircleShader;
}


void R_DrawParticles (void)
{
	D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
	D3D11_MAPPED_SUBRESOURCE msr;

	if (!r_newrefdef.num_particles)
		return;

	// square particles can potentially expose a faster path by not using alpha blending
	// but we might wish to add particle fade at some time so we can't do it (note: all particles in Q2 have fade)
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSDepthNoWrite, d3d_RSFullCull);
	D_BindShaderBundle (d3d_ParticleShader);
	D_BindVertexBuffer (0, d3d_ParticleVertexes, sizeof (particle_t), 0);

	if (r_FirstParticle + r_newrefdef.num_particles >= MAX_GPU_PARTICLES)
	{
		r_FirstParticle = 0;
		mode = D3D11_MAP_WRITE_DISCARD;
	}

	if (SUCCEEDED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) d3d_ParticleVertexes, 0, mode, 0, &msr)))
	{
		// copy over the particles and unmap the buffer
		memcpy ((particle_t *) msr.pData + r_FirstParticle, r_newrefdef.particles, r_newrefdef.num_particles * sizeof (particle_t));
		d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) d3d_ParticleVertexes, 0);

		// go to points for the geometry shader
		d3d_Context->lpVtbl->IASetPrimitiveTopology (d3d_Context, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		// and draw it
		d3d_Context->lpVtbl->Draw (d3d_Context, r_newrefdef.num_particles, r_FirstParticle);

		// back to triangles
		d3d_Context->lpVtbl->IASetPrimitiveTopology (d3d_Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// and go to the next particle batch
		r_FirstParticle += r_newrefdef.num_particles;
	}

	r_newrefdef.num_particles = 0;
}


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// sprites
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct spriteconstants_s {
	float spriteOrigin[3];
	float spritealpha;
	float xywh[4];
} spriteconstants_t;


int d3d_SpriteShader;
ID3D11Buffer *d3d_SpriteConstants;


void R_InitSprites (void)
{
	D3D11_BUFFER_DESC cbPerSpriteDesc = {
		sizeof (spriteconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	d3d_SpriteShader = D_CreateShaderBundle (IDR_MISCSHADER, "SpriteVS", "SpriteGS", "SpritePS", NULL, 0);

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbPerSpriteDesc, NULL, &d3d_SpriteConstants);
	D_RegisterConstantBuffer (d3d_SpriteConstants, 6);
}


void R_UpdateSpriteconstants (entity_t *e, dsprframe_t *frame)
{
	spriteconstants_t consts;

	Vector3Copy (consts.spriteOrigin, e->currorigin);

	if (e->flags & RF_TRANSLUCENT)
		consts.spritealpha = e->alpha;
	else consts.spritealpha = 1;

	consts.xywh[0] = -frame->origin_x;
	consts.xywh[1] = -frame->origin_y;
	consts.xywh[2] = frame->width - frame->origin_x;
	consts.xywh[3] = frame->height - frame->origin_y;

	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_SpriteConstants, 0, NULL, &consts, 0, 0);
}


void R_DrawSpriteModel (entity_t *e)
{
	model_t *mod = e->model;

	// don't even bother culling, because it's just a single polygon without a surface cache
	// (note - with hardware it might make sense to cull)
	dsprite_t *psprite = mod->sprheader;
	int framenum = e->currframe % psprite->numframes;
	dsprframe_t *frame = &psprite->frames[framenum];

	R_UpdateSpriteconstants (e, frame);

	D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSFullCull);
	D_BindShaderBundle (d3d_SpriteShader);
	R_BindTexture (mod->skins[framenum]->SRV);

	// we'll do triangle-to-quad expansion in our geometry shader
	d3d_Context->lpVtbl->Draw (d3d_Context, 3, 0);
}


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// sky
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
char	skyname[MAX_QPATH];
float	skyrotate;
vec3_t	skyaxis;

texture_t r_SkyCubemap;

static int d3d_SurfDrawSkyShader;
static int d3d_SkyNoSkyShader;


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


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// underwater warp
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
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


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// underwater warp/sky common
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
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


// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// state objects
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
#define MAX_VERTEX_STREAMS	16

typedef struct streamdef_s {
	ID3D11Buffer *Buffer;
	UINT Stride;
	UINT Offset;
} streamdef_t;


ID3D11SamplerState *D_CreateSamplerState (D3D11_FILTER Filter, D3D11_TEXTURE_ADDRESS_MODE AddressMode, float MaxLOD, UINT MaxAnisotropy)
{
	ID3D11SamplerState *ss = NULL;
	D3D11_SAMPLER_DESC desc;

	desc.AddressU = AddressMode;
	desc.AddressV = AddressMode;
	desc.AddressW = AddressMode;

	// border colour is always black because that's what our clamp-to-border code elsewhere in the engine expects
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0;

	// nope, not doing this
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	if (MaxAnisotropy > 1)
	{
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.MaxAnisotropy = MaxAnisotropy;
	}
	else
	{
		desc.Filter = Filter;
		desc.MaxAnisotropy = 1;
	}

	desc.MaxLOD = MaxLOD;
	desc.MinLOD = 0;
	desc.MipLODBias = 0;

	d3d_Device->lpVtbl->CreateSamplerState (d3d_Device, &desc, &ss);
	D_CacheObject ((ID3D11DeviceChild *) ss, "ID3D11SamplerState");

	return ss;
}


ID3D11BlendState *D_CreateBlendState (BOOL blendon, D3D11_BLEND src, D3D11_BLEND dst, D3D11_BLEND_OP op)
{
	ID3D11BlendState *bs = NULL;
	D3D11_BLEND_DESC desc;

	desc.AlphaToCoverageEnable = FALSE;
	desc.IndependentBlendEnable = FALSE;
	desc.RenderTarget[0].BlendEnable = blendon;
	desc.RenderTarget[0].SrcBlend = src;
	desc.RenderTarget[0].DestBlend = dst;
	desc.RenderTarget[0].BlendOp = op;
	desc.RenderTarget[0].SrcBlendAlpha = src;
	desc.RenderTarget[0].DestBlendAlpha = dst;
	desc.RenderTarget[0].BlendOpAlpha = op;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	d3d_Device->lpVtbl->CreateBlendState (d3d_Device, &desc, &bs);
	D_CacheObject ((ID3D11DeviceChild *) bs, "ID3D11BlendState");

	return bs;
}


ID3D11DepthStencilState *D_CreateDepthState (BOOL test, BOOL mask, D3D11_COMPARISON_FUNC func)
{
	// to do - add an enum controlling the stencil test; DS_STNONE, DS_STZPASS, DS_STZFAIL, etc...
	ID3D11DepthStencilState *ds = NULL;
	D3D11_DEPTH_STENCIL_DESC desc;

	if (test)
	{
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = mask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	}
	else
	{
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	}

	desc.DepthFunc = func;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

	d3d_Device->lpVtbl->CreateDepthStencilState (d3d_Device, &desc, &ds);
	D_CacheObject ((ID3D11DeviceChild *) ds, "ID3D11DepthStencilState");

	return ds;
}


ID3D11RasterizerState *D_CreateRasterizerState (D3D11_FILL_MODE fill, D3D11_CULL_MODE cull, BOOL clip, BOOL scissor)
{
	ID3D11RasterizerState *rs = NULL;
	D3D11_RASTERIZER_DESC desc;

	desc.FillMode = fill;
	desc.CullMode = cull;
	desc.FrontCounterClockwise = TRUE;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0;
	desc.SlopeScaledDepthBias = 0;
	desc.DepthClipEnable = clip;
	desc.ScissorEnable = scissor;
	desc.MultisampleEnable = FALSE;
	desc.AntialiasedLineEnable = FALSE;

	d3d_Device->lpVtbl->CreateRasterizerState (d3d_Device, &desc, &rs);
	D_CacheObject ((ID3D11DeviceChild *) rs, "ID3D11RasterizerState");

	return rs;
}


ID3D11BlendState *d3d_BSNone = NULL;
ID3D11BlendState *d3d_BSAlphaBlend = NULL;
ID3D11BlendState *d3d_BSAlphaReverse = NULL;
ID3D11BlendState *d3d_BSAlphaPreMult = NULL;
ID3D11BlendState *d3d_BSAdditive = NULL;
ID3D11BlendState *d3d_BSSubtractive = NULL;

ID3D11DepthStencilState *d3d_DSFullDepth = NULL;
ID3D11DepthStencilState *d3d_DSDepthNoWrite = NULL;
ID3D11DepthStencilState *d3d_DSNoDepth = NULL;
ID3D11DepthStencilState *d3d_DSEqualDepthNoWrite = NULL;

ID3D11RasterizerState *d3d_RSFullCull = NULL;
ID3D11RasterizerState *d3d_RSReverseCull = NULL;
ID3D11RasterizerState *d3d_RSNoCull = NULL;

ID3D11SamplerState *d3d_MainSampler = NULL;
ID3D11SamplerState *d3d_LMapSampler = NULL;
ID3D11SamplerState *d3d_WarpSampler = NULL;
ID3D11SamplerState *d3d_DrawSampler = NULL;
ID3D11SamplerState *d3d_CineSampler = NULL;


void GL_SetDefaultState (void)
{
	d3d_BSNone = D_CreateBlendState (FALSE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAlphaBlend = D_CreateBlendState (TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAlphaReverse = D_CreateBlendState (TRUE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAlphaPreMult = D_CreateBlendState (TRUE, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAdditive = D_CreateBlendState (TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD);
	d3d_BSSubtractive = D_CreateBlendState (TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_REV_SUBTRACT);

	d3d_DSFullDepth = D_CreateDepthState (TRUE, TRUE, D3D11_COMPARISON_LESS_EQUAL);
	d3d_DSDepthNoWrite = D_CreateDepthState (TRUE, FALSE, D3D11_COMPARISON_LESS_EQUAL);
	d3d_DSNoDepth = D_CreateDepthState (FALSE, FALSE, D3D11_COMPARISON_ALWAYS);
	d3d_DSEqualDepthNoWrite = D_CreateDepthState (TRUE, FALSE, D3D11_COMPARISON_EQUAL);

	d3d_RSFullCull = D_CreateRasterizerState (D3D11_FILL_SOLID, D3D11_CULL_FRONT, TRUE, FALSE);
	d3d_RSReverseCull = D_CreateRasterizerState (D3D11_FILL_SOLID, D3D11_CULL_BACK, TRUE, FALSE);
	d3d_RSNoCull = D_CreateRasterizerState (D3D11_FILL_SOLID, D3D11_CULL_NONE, TRUE, FALSE);

	d3d_MainSampler = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FLOAT32_MAX, 16);
	d3d_LMapSampler = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1);
	d3d_WarpSampler = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, 0, 1);
	d3d_DrawSampler = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1);
	d3d_CineSampler = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_BORDER, 0, 1);
}


ID3D11RasterizerState *R_GetRasterizerState (int rflags)
{
	if ((rflags & RF_WEAPONMODEL) && r_lefthand->value)
		return d3d_RSReverseCull;
	else return d3d_RSFullCull;
}


void D_BindSamplers (void)
{
	// rebind in case state gets chucked
	ID3D11SamplerState *Samplers[] = {
		d3d_MainSampler,
		d3d_LMapSampler,
		d3d_WarpSampler,
		d3d_DrawSampler,
		d3d_CineSampler
	};

	d3d_Context->lpVtbl->PSSetSamplers (d3d_Context, 0, 5, Samplers);
}


void D_SetRenderStates (ID3D11BlendState *bs, ID3D11DepthStencilState *ds, ID3D11RasterizerState *rs)
{
	static ID3D11BlendState *oldbs = NULL;
	static ID3D11DepthStencilState *oldds = NULL;
	static ID3D11RasterizerState *oldrs = NULL;

	if (oldbs != bs)
	{
		d3d_Context->lpVtbl->OMSetBlendState (d3d_Context, bs, NULL, 0xffffffff);
		oldbs = bs;
	}

	if (oldds != ds)
	{
		d3d_Context->lpVtbl->OMSetDepthStencilState (d3d_Context, ds, 0);
		oldds = ds;
	}

	if (oldrs != rs)
	{
		d3d_Context->lpVtbl->RSSetState (d3d_Context, rs);
		oldrs = rs;
	}
}


void D_SetInputLayout (ID3D11InputLayout *il)
{
	static ID3D11InputLayout *oldil = NULL;

	if (oldil != il)
	{
		d3d_Context->lpVtbl->IASetInputLayout (d3d_Context, il);
		oldil = il;
	}
}


void D_SetVertexShader (ID3D11VertexShader *vs)
{
	static ID3D11VertexShader *oldvs = NULL;

	if (oldvs != vs)
	{
		d3d_Context->lpVtbl->VSSetShader (d3d_Context, vs, NULL, 0);
		oldvs = vs;
	}
}


void D_SetGeometryShader (ID3D11GeometryShader *gs)
{
	static ID3D11GeometryShader *oldgs = NULL;

	if (oldgs != gs)
	{
		d3d_Context->lpVtbl->GSSetShader (d3d_Context, gs, NULL, 0);
		oldgs = gs;
	}
}


void D_SetPixelShader (ID3D11PixelShader *ps)
{
	static ID3D11PixelShader *oldps = NULL;

	if (oldps != ps)
	{
		d3d_Context->lpVtbl->PSSetShader (d3d_Context, ps, NULL, 0);
		oldps = ps;
	}
}


void D_BindVertexBuffer (UINT Slot, ID3D11Buffer *Buffer, UINT Stride, UINT Offset)
{
	static streamdef_t d3d_VertexStreams[MAX_VERTEX_STREAMS];

	if (Slot < 0) return;
	if (Slot >= MAX_VERTEX_STREAMS) return;

	if (d3d_VertexStreams[Slot].Buffer != Buffer ||
		d3d_VertexStreams[Slot].Stride != Stride ||
		d3d_VertexStreams[Slot].Offset != Offset)
	{
		d3d_Context->lpVtbl->IASetVertexBuffers (d3d_Context, Slot, 1, &Buffer, &Stride, &Offset);

		d3d_VertexStreams[Slot].Buffer = Buffer;
		d3d_VertexStreams[Slot].Stride = Stride;
		d3d_VertexStreams[Slot].Offset = Offset;
	}
}


void D_BindIndexBuffer (ID3D11Buffer *Buffer, DXGI_FORMAT Format)
{
	static ID3D11Buffer *OldBuffer = NULL;
	static DXGI_FORMAT OldFormat = DXGI_FORMAT_UNKNOWN;

	if (OldBuffer != Buffer || OldFormat != Format)
	{
		d3d_Context->lpVtbl->IASetIndexBuffer (d3d_Context, Buffer, Format, 0);

		OldBuffer = Buffer;
		OldFormat = Format;
	}
}

