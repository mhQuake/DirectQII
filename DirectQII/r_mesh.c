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
// gl_mesh.c: triangle model functions

#include "r_local.h"
#include "mesh.h"


static int d3d_MeshLightmapShader;
static int d3d_MeshDynamicShader;
static int d3d_MeshPowersuitShader;
static int d3d_MeshFullbrightShader;

static ID3D11Buffer *d3d_MeshConstants = NULL;

void R_InitMesh (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0),
		VDECL ("NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0),
		VDECL ("POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0),
		VDECL ("NORMAL",   1, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    3, 0)
	};

	D3D11_BUFFER_DESC cbPerMeshDesc = {
		sizeof (meshconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	d3d_MeshLightmapShader = D_CreateShaderBundle (IDR_MESHSHADER, "MeshLightmapVS", NULL, "MeshLightmapPS", DEFINE_LAYOUT (layout));
	d3d_MeshDynamicShader = D_CreateShaderBundle (IDR_MESHSHADER, "MeshDynamicVS", NULL, "GenericDynamicPS", DEFINE_LAYOUT (layout));
	d3d_MeshPowersuitShader = D_CreateShaderBundle (IDR_MESHSHADER, "MeshPowersuitVS", NULL, "MeshPowersuitPS", DEFINE_LAYOUT (layout));
	d3d_MeshFullbrightShader = D_CreateShaderBundle (IDR_MESHSHADER, "MeshLightmapVS", NULL, "MeshFullbrightPS", DEFINE_LAYOUT (layout));

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbPerMeshDesc, NULL, &d3d_MeshConstants);
	D_RegisterConstantBuffer (d3d_MeshConstants, 3);

	// init vertex cache optimization
	VCache_Init ();
}


/*
==============================================================================

MESH COMMON FUNCTIONS

==============================================================================
*/


void R_SelectMeshShader (int eflags)
{
	// figure the correct shaders to use
	if (eflags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
		D_BindShaderBundle (d3d_MeshPowersuitShader);
	else if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		D_BindShaderBundle (d3d_MeshFullbrightShader);
	else if (!r_worldmodel->lightdata || r_fullbright->value)
		D_BindShaderBundle (d3d_MeshFullbrightShader);
	else D_BindShaderBundle (d3d_MeshLightmapShader);
}


image_t *R_SelectMeshTexture (entity_t *e, model_t *mod)
{
	// nasty shit
	image_t *skin = NULL;

	// select skin
	if (r_testnotexture->value)
		return r_notexture;
	if (r_lightmap->value)
		return r_greytexture;
	else if (e->skin)
		skin = e->skin;	// custom player skin (this may be NULL so we can't just return it)
	else
	{
		if (e->skinnum >= MAX_MD2SKINS)
			skin = mod->skins[0];
		else
		{
			if ((skin = mod->skins[e->skinnum]) == NULL)
				skin = mod->skins[0];
		}
	}

	if (!skin)
		skin = r_notexture;	// fallback...

	return skin;
}


void R_DrawMeshPolySet (model_t *mod)
{
	bufferset_t *set = R_GetBufferSetForIndex (mod->bufferset);
	d3d_Context->lpVtbl->DrawIndexed (d3d_Context, set->numindexes, 0, 0);
}


qboolean R_MeshLightInteraction (entity_t *e, model_t *mod, dlight_t *dl)
{
	// sphere/sphere intersection test
	return (Vector3Dist (e->currorigin, dl->origin) < (dl->radius + mod->radius));
}


void R_MeshDlights (entity_t *e, model_t *mod, QMATRIX *localMatrix)
{
	for (int i = 0; i < r_newrefdef.num_dlights; i++)
	{
		dlight_t *dl = &r_newrefdef.dlights[i];

		// a dl that's been culled will have it's intensity set to 0
		if (!(dl->radius > 0)) continue;

		if (R_MeshLightInteraction (e, mod, dl))
		{
			// move the light into entity local space
			float transformedorigin[3];
			R_VectorInverseTransform (localMatrix, transformedorigin, dl->origin);

			// set up the light
			D_SetupDynamicLight (dl, transformedorigin, e->flags);

			// set up the shaders
			D_BindShaderBundle (d3d_MeshDynamicShader);

			// and draw it
			R_DrawMeshPolySet (mod);
		}
	}

	// go to a new dlight frame for each push so that we don't carry over lights from the previous
	r_dlightframecount++;
}


void R_SetupMeshFrameLerp (entity_t *e, model_t *mod)
{
	// get the buffer set used by this model
	bufferset_t *set = R_GetBufferSetForIndex (mod->bufferset);

	// sets up stuff that's going to be valid for both the main pass and the dynamic lighting pass(es)
	R_BindTexture (R_SelectMeshTexture (e, mod)->SRV);

	D_BindVertexBuffer (1, set->PositionsBuffer, sizeof (meshpolyvert_t), e->prevframe * set->framevertexstride);
	D_BindVertexBuffer (2, set->PositionsBuffer, sizeof (meshpolyvert_t), e->currframe * set->framevertexstride);
	D_BindVertexBuffer (3, set->TexCoordsBuffer, sizeof (float) * 2, 0);

	D_BindIndexBuffer (set->IndexBuffer, DXGI_FORMAT_R16_UINT);
}


void R_TransformMeshEntity (entity_t *e, meshconstants_t *consts, QMATRIX *localmatrix)
{
	// set up the lerp transform
	float move[3], delta[3];
	float frontlerp = 1.0 - e->backlerp;

	// move should be the delta back to the previous frame * backlerp
	Vector3Subtract (delta, e->prevorigin, e->currorigin);

	// take these from the local matrix
	move[0] = Vector3Dot (delta, localmatrix->m4x4[0]);	// forward
	move[1] = Vector3Dot (delta, localmatrix->m4x4[1]);	// left
	move[2] = Vector3Dot (delta, localmatrix->m4x4[2]);	// up

	// translate and scale factors were baked into the vertex buffer data so set these to identity
	// we can't modify the frame data as it's still used for bboxes
	Vector3Set (consts->move, e->backlerp * move[0], e->backlerp * move[1], e->backlerp * move[2]);
	Vector3Set (consts->frontv, frontlerp, frontlerp, frontlerp);
	Vector3Set (consts->backv, e->backlerp, e->backlerp, e->backlerp);

	if (!(e->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)))
		consts->suitscale = 0;
	else consts->suitscale = POWERSUIT_SCALE;

	consts->backlerp = e->backlerp;
}


void R_LightMeshEntity (entity_t *e, meshconstants_t *consts, QMATRIX *localmatrix)
{
	// optionally allow using the software renderer light vector; we don't do this by default because it messes up some of the
	// lighting on certain objects (it wouldn't if we were using the full software renderer lighting model)
	if (r_lightmodel->value)
	{
		// this is the light vector from the GL renderer
		float an = (e->angles[0] + e->angles[1]) / 180 * M_PI;

		consts->shadevector[0] = cos (-an);
		consts->shadevector[1] = sin (-an);
		consts->shadevector[2] = 1;
	}
	else
	{
		// this is the light vector from the software renderer
		float lightvec[3] = {-1, 0, 0};

		consts->shadevector[0] = Vector3Dot (lightvec, localmatrix->m4x4[0]);
		consts->shadevector[1] = Vector3Dot (lightvec, localmatrix->m4x4[1]);
		consts->shadevector[2] = Vector3Dot (lightvec, localmatrix->m4x4[2]);
	}

	Vector3Normalize (consts->shadevector);

	// clear the dynamic lighting flag
	e->flags &= ~RF_DYNAMICLIGHT;

	// PGM	ir goggles override
	if ((r_newrefdef.rdflags & RDF_IRGOGGLES) && (e->flags & RF_IR_VISIBLE))
	{
		consts->shadelight[0] = 1.0;
		consts->shadelight[1] = 0.0;
		consts->shadelight[2] = 0.0;
		return;
	}

	// get lighting information
	// PMM - rewrote, reordered to handle new shells & mixing
	if (e->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		// PMM -special case for godmode
		if ((e->flags & RF_SHELL_RED) && (e->flags & RF_SHELL_BLUE) && (e->flags & RF_SHELL_GREEN))
		{
			consts->shadelight[0] = 1.0;
			consts->shadelight[1] = 1.0;
			consts->shadelight[2] = 1.0;
		}
		else if (e->flags & (RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
		{
			Vector3Clear (consts->shadelight);

			if (e->flags & RF_SHELL_RED)
			{
				consts->shadelight[0] = 1.0;

				if (e->flags & (RF_SHELL_BLUE | RF_SHELL_DOUBLE))
					consts->shadelight[2] = 1.0;
			}
			else if (e->flags & RF_SHELL_BLUE)
			{
				if (e->flags & RF_SHELL_DOUBLE)
				{
					consts->shadelight[1] = 1.0;
					consts->shadelight[2] = 1.0;
				}
				else consts->shadelight[2] = 1.0;
			}
			else if (e->flags & RF_SHELL_DOUBLE)
			{
				consts->shadelight[0] = 0.9;
				consts->shadelight[1] = 0.7;
			}
		}
		else if (e->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN))
		{
			Vector3Clear (consts->shadelight);

			// PMM - new colors
			if (e->flags & RF_SHELL_HALF_DAM)
			{
				consts->shadelight[0] = 0.56;
				consts->shadelight[1] = 0.59;
				consts->shadelight[2] = 0.45;
			}

			if (e->flags & RF_SHELL_GREEN)
				consts->shadelight[1] = 1.0;
		}
	}
	else if (e->flags & RF_FULLBRIGHT)
	{
		consts->shadelight[0] = 1.0;
		consts->shadelight[1] = 1.0;
		consts->shadelight[2] = 1.0;
	}
	else
	{
		// don't add dynamics here because we'll blend them in using extra passes later on
		// doing this allows large models (such as flags) recieve dlights properly rather than just being fully lit
		// (null models and R_SetLightLevel still use R_DynamicLightPoint)
		R_LightPoint (e->currorigin, consts->shadelight);

		// this entity now gets dynamic lights
		e->flags |= RF_DYNAMICLIGHT;

		// player lighting level was fixed in R_SetLightLevel so it's no longer necessary to do it here
		// it never worked right in R_SetLightLevel because the original code used "currententity" all
		// over the place, which was horrible.
	}

	if (e->flags & RF_MINLIGHT)
	{
		float scale = 8.0f / 255.0f;

		consts->shadelight[0] = (consts->shadelight[0] + scale) / (1.0f + scale);
		consts->shadelight[1] = (consts->shadelight[1] + scale) / (1.0f + scale);
		consts->shadelight[2] = (consts->shadelight[2] + scale) / (1.0f + scale);
	}

	if (e->flags & RF_GLOW)
	{
		// bonus items will pulse with time
		float scale = 0.05f + 0.05f * sinf (r_newrefdef.time * 7.0f);

		for (int i = 0; i < 3; i++)
		{
			float min = consts->shadelight[i] * 0.8;

			consts->shadelight[i] += scale;

			if (consts->shadelight[i] < min)
				consts->shadelight[i] = min;
		}
	}
}


static qboolean R_CullMeshEntity (entity_t *e, QMATRIX *localmatrix)
{
	model_t *mod = e->model;

	if (mod->type == mod_alias)
	{
		mmdl_t *hdr = mod->md2header;

		// the frames were fixed-up in R_PrepareAliasModel so we don't need to do so here
		maliasframe_t *currframe = &hdr->frames[e->currframe];
		maliasframe_t *prevframe = &hdr->frames[e->prevframe];

		// compute axially aligned mins and maxs
		if (currframe == prevframe)
		{
			for (int i = 0; i < 3; i++)
			{
				mod->mins[i] = currframe->translate[i];
				mod->maxs[i] = mod->mins[i] + currframe->scale[i] * 255;
			}
		}
		else
		{
			for (int i = 0; i < 3; i++)
			{
				float currmins = currframe->translate[i];
				float currmaxs = currmins + currframe->scale[i] * 255;

				float prevmins = prevframe->translate[i];
				float prevmaxs = prevmins + prevframe->scale[i] * 255;

				if (currmins < prevmins) mod->mins[i] = currmins; else mod->mins[i] = prevmins;
				if (currmaxs > prevmaxs) mod->maxs[i] = currmaxs; else mod->maxs[i] = prevmaxs;
			}
		}
	}
	else if (mod->type == mod_md5)
	{
		md5header_t *hdr = mod->md5header;

		// compute axially aligned mins and maxs
		for (int i = 0; i < 3; i++)
		{
			float currmins = hdr->bboxes[e->currframe].mins[i];
			float currmaxs = hdr->bboxes[e->currframe].maxs[i];

			float prevmins = hdr->bboxes[e->prevframe].mins[i];
			float prevmaxs = hdr->bboxes[e->prevframe].maxs[i];

			if (currmins < prevmins) mod->mins[i] = currmins; else mod->mins[i] = prevmins;
			if (currmaxs > prevmaxs) mod->maxs[i] = currmaxs; else mod->maxs[i] = prevmaxs;
		}
	}

	// take a radius for dlight interaction tests
	mod->radius = Mod_RadiusFromBounds (mod->mins, mod->maxs);

	// and run the cull
	return R_CullForEntity (mod->mins, mod->maxs, localmatrix);
}


void R_DrawMeshEntity (entity_t *e, QMATRIX *localmatrix)
{
	model_t		*mod = e->model;

	// per-mesh cbuffer constants
	meshconstants_t consts;

	if (!(e->flags & RF_WEAPONMODEL))
	{
		if (R_CullMeshEntity (e, localmatrix))
			return;
	}
	else
	{
		if (r_lefthand->value == 2)
			return;
	}

	// cbuffers/states
	R_PrepareEntityForRendering (localmatrix, NULL, e->alpha, e->flags);

	// set up our mesh constants
	R_LightMeshEntity (e, &consts, localmatrix);
	R_TransformMeshEntity (e, &consts, localmatrix);

	// the new lighting is slightly darker than the old so scale it up to compensate; this is a good working range
	Vector3Scalef (consts.shadelight, consts.shadelight, 1.28f);

	// and update to the cbuffer
	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_MeshConstants, 0, NULL, &consts, 0, 0);

	// set up the frame interpolation
	R_SetupMeshFrameLerp (e, mod);

	// select the correct shader to use for the main pass
	R_SelectMeshShader (e->flags);

	// and draw it
	R_DrawMeshPolySet (mod);

	// no dlights in the player setup screens
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;

	// no dlights in fullbright mode
	if (!r_worldmodel->lightdata || r_fullbright->value) return;

	// no dlights unless it's flagged for them
	if (!(e->flags & RF_DYNAMICLIGHT)) return;

	// add dynamic lighting to the entity
	R_MeshDlights (e, mod, localmatrix);
}


void R_PrepareAliasModel (entity_t *e, QMATRIX *localmatrix)
{
	model_t		*mod = e->model;
	mmdl_t		*hdr = mod->md2header;

	// fix up the frames in advance of culling because it needs them
	if ((e->currframe >= hdr->num_frames) || (e->currframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_PrepareAliasModel %s: no such currframe %d\n", mod->name, e->currframe);
		e->currframe = 0;
		e->prevframe = 0;
	}

	if ((e->prevframe >= hdr->num_frames) || (e->prevframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_PrepareAliasModel %s: no such prevframe %d\n", mod->name, e->prevframe);
		e->currframe = 0;
		e->prevframe = 0;
	}

	// get the transform in local space so that we can correctly handle dlights
	R_MatrixIdentity (localmatrix);
	R_MatrixTranslate (localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotate (localmatrix, e->angles[0], e->angles[1], -e->angles[2]);
}


void R_PrepareMD5Model (entity_t *e, QMATRIX *localmatrix)
{
	model_t *mod = e->model;
	md5header_t *hdr = mod->md5header;

	// fix up the frames in advance of culling because it needs them
	if ((e->currframe >= hdr->num_frames) || (e->currframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_PrepareMD5Model %s: no such currframe %d\n", mod->name, e->currframe);
		e->currframe = 0;
		e->prevframe = 0;
	}

	if ((e->prevframe >= hdr->num_frames) || (e->prevframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_PrepareMD5Model %s: no such prevframe %d\n", mod->name, e->prevframe);
		e->currframe = 0;
		e->prevframe = 0;
	}

	// get the transform in local space so that we can correctly handle dlights
	R_MatrixIdentity (localmatrix);
	R_MatrixTranslate (localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotate (localmatrix, e->angles[0], e->angles[1], -e->angles[2]);
}

