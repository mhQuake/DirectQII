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


typedef struct aliasbuffers_s {
	ID3D11Buffer *PolyVerts;
	ID3D11Buffer *TexCoords;
	ID3D11Buffer *Indexes;

	int NumVerts;
	int NumIndexes;
	int NumTris;

	char Name[256];
	int registration_sequence;
} aliasbuffers_t;

typedef struct meshconstants_s {
	float shadelight[4];	// padded for cbuffer
	float shadevector[4];	// padded for cbuffer
	float move[4];			// padded for cbuffer
	float frontv[4];		// padded for cbuffer
	float backv[4];			// padded for cbuffer
	float suitscale;
	float backlerp;
	float junk[2];			// cbuffer padding
} meshconstants_t;

typedef struct aliaspolyvert_s {
	// xyz is position
	// w is lightnormalindex
	byte position[4];
} aliaspolyvert_t;

typedef struct aliastexcoord_s {
	float texcoord[2];
} aliastexcoord_t;

static aliasbuffers_t d3d_AliasBuffers[MAX_MOD_KNOWN];

static int d3d_MeshLightmapShader;
static int d3d_MeshDynamicShader;
static int d3d_MeshPowersuitShader;
static int d3d_MeshFullbrightShader;

static ID3D11Buffer *d3d_MeshConstants = NULL;

void R_InitMesh (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("PREVTRIVERTX", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 0),
		VDECL ("CURRTRIVERTX", 0, DXGI_FORMAT_R8G8B8A8_UINT, 1, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0)
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
}


void R_ShutdownMesh (void)
{
	int i;

	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		SAFE_RELEASE (set->PolyVerts);
		SAFE_RELEASE (set->TexCoords);
		SAFE_RELEASE (set->Indexes);

		memset (set, 0, sizeof (aliasbuffers_t));
	}
}


void R_FreeUnusedAliasBuffers (void)
{
	int i;

	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		if (set->registration_sequence != r_registration_sequence)
		{
			SAFE_RELEASE (set->PolyVerts);
			SAFE_RELEASE (set->TexCoords);
			SAFE_RELEASE (set->Indexes);

			memset (set, 0, sizeof (aliasbuffers_t));
		}
	}
}


void D_MakeAliasPolyverts (aliasbuffers_t *set, int numposes, int nummesh, aliaspolyvert_t *polyverts)
{
	D3D11_BUFFER_DESC vbDesc = {
		sizeof (aliaspolyvert_t) * numposes * nummesh,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = {polyverts, 0, 0};

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->PolyVerts);
}


void D_SetupAliasPolyVerts (dmdl_t *hdr, aliasbuffers_t *set)
{
	aliaspolyvert_t *polyverts = ri.Load_AllocMemory (set->NumVerts * hdr->num_frames * sizeof (aliaspolyvert_t));
	aliaspolyvert_t *verts = polyverts;
	int f;

	for (f = 0; f < hdr->num_frames; f++)
	{
		// retrieve the current frame
		int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);
		daliasframe_t *frame = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + f * hdr->framesize);

		// and now build the data for this frame
		for (;;)
		{
			// get the vertex count and primitive type
			int i, count = *order++;

			if (!count) break;
			if (count < 0) count = -count;

			for (i = 0; i < count; i++, verts++, order += 3)
			{
				// copy over the data
				verts->position[0] = frame->verts[order[2]].v[0];
				verts->position[1] = frame->verts[order[2]].v[1];
				verts->position[2] = frame->verts[order[2]].v[2];
				verts->position[3] = frame->verts[order[2]].lightnormalindex;
			}
		}
	}

	D_MakeAliasPolyverts (set, hdr->num_frames, set->NumVerts, polyverts);
	ri.Load_FreeMemory ();
}


void D_MakeAliasTexCoords (aliasbuffers_t *set, int numtexcoords, aliastexcoord_t *texcoords)
{
	D3D11_BUFFER_DESC vbDesc = {
		sizeof (aliastexcoord_t) * numtexcoords,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = {texcoords, 0, 0};

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->TexCoords);
}


void D_SetupAliasTexCoords (dmdl_t *hdr, aliasbuffers_t *set)
{
	aliastexcoord_t *texcoords = ri.Load_AllocMemory (set->NumVerts * sizeof (aliastexcoord_t));
	aliastexcoord_t *st = texcoords;

	int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);

	for (;;)
	{
		// get the vertex count and primitive type
		int i, count = *order++;

		if (!count) break;
		if (count < 0) count = -count;

		for (i = 0; i < count; i++, st++, order += 3)
		{
			st->texcoord[0] = ((float *) order)[0];
			st->texcoord[1] = ((float *) order)[1];
		}
	}

	D_MakeAliasTexCoords (set, set->NumVerts, texcoords);
	ri.Load_FreeMemory ();
}


void D_MakeAliasIndexes (aliasbuffers_t *set, int numindexes, unsigned short *indexes)
{
	D3D11_BUFFER_DESC ibDesc = {
		sizeof (unsigned short) * numindexes,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = {indexes, 0, 0};

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &set->Indexes);
}


void D_SetupAliasIndexes (dmdl_t *hdr, aliasbuffers_t *set)
{
	unsigned short *indexes = ri.Load_AllocMemory (set->NumIndexes * sizeof (unsigned short));
	unsigned short *ndx = indexes;

	int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);
	int i, firstvertex = 0, numindexes = 0;

	for (;;)
	{
		// get the vertex count and primitive type
		int count = *order++;
		if (!count) break;

		if (count < 0)
		{
			for (i = 2, count = -count; i < count; i++, ndx += 3, numindexes += 3)
			{
				ndx[0] = firstvertex + 0;
				ndx[1] = firstvertex + (i - 1);
				ndx[2] = firstvertex + i;
			}
		}
		else
		{
			for (i = 2; i < count; i++, ndx += 3, numindexes += 3)
			{
				ndx[0] = firstvertex + i - 2;
				ndx[1] = firstvertex + ((i & 1) ? i : (i - 1));
				ndx[2] = firstvertex + ((i & 1) ? (i - 1) : i);
			}
		}

		order += count * 3;
		firstvertex += count;
	}

	D_MakeAliasIndexes (set, numindexes, indexes);
	ri.Load_FreeMemory ();
}


void D_CreateAliasBufferSet (model_t *mod, dmdl_t *hdr)
{
	// get the counts for everything we need
	int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);

	aliasbuffers_t *set = &d3d_AliasBuffers[mod->bufferset];

	set->NumVerts = 0;
	set->NumTris = 0;

	for (;;)
	{
		// get the vertex count and primitive type
		int count = *order++;

		if (!count) break;
		if (count < 0) count = -count;

		order += (count * 3);

		set->NumTris += (count - 2);
		set->NumVerts += count;
	}

	// detrive the correct index count
	set->NumIndexes = set->NumTris * 3;

	// and build them all
	D_SetupAliasPolyVerts (hdr, set);
	D_SetupAliasTexCoords (hdr, set);
	D_SetupAliasIndexes (hdr, set);
}


int D_FindAliasBuffers (model_t *mod)
{
	int i;

	// see do we already have it
	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		if (!set->PolyVerts) continue;
		if (!set->TexCoords) continue;
		if (!set->Indexes) continue;

		if (strcmp (set->Name, mod->name)) continue;

		// use this set and mark it as active
		mod->bufferset = i;
		set->registration_sequence = r_registration_sequence;

		return mod->bufferset;
	}

	return -1;
}


void D_MakeAliasBuffers (model_t *mod)
{
	int i;

	// see do we already have it
	if ((mod->bufferset = D_FindAliasBuffers (mod)) != -1) return;

	// find the first free buffer
	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		// already allocated
		if (set->PolyVerts) continue;
		if (set->TexCoords) continue;
		if (set->Indexes) continue;

		// cache the name so that we'll find it next time too
		strcpy (set->Name, mod->name);

		// use this set and mark it as active
		mod->bufferset = i;
		set->registration_sequence = r_registration_sequence;

		// now build everything from the model data
		D_CreateAliasBufferSet (mod, (dmdl_t *) mod->extradata);

		// and done
		return;
	}

	ri.Sys_Error (ERR_DROP, "D_MakeAliasBuffers : not enough free buffers!");
}


image_t *R_GetAliasSkin (entity_t *e, model_t *mod)
{
	// nasty shit
	image_t	*skin = NULL;

	// select skin
	if (r_lightmap->value)
		skin = r_greytexture;
	else if (e->skin)
		skin = e->skin;	// custom player skin
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


void GL_DrawAliasPolySet (model_t *mod)
{
	aliasbuffers_t *set = &d3d_AliasBuffers[mod->bufferset];
	d3d_Context->lpVtbl->DrawIndexed (d3d_Context, set->NumIndexes, 0, 0);
}


void GL_SetupAliasFrameLerp (entity_t *e, model_t *mod, aliasbuffers_t *set)
{
	R_BindTexture (R_GetAliasSkin (e, mod)->SRV);

	// figure the correct shaders to use
	if (e->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
		D_BindShaderBundle (d3d_MeshPowersuitShader);
	else if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		D_BindShaderBundle (d3d_MeshFullbrightShader);
	else if (!r_worldmodel->lightdata || r_fullbright->value)
		D_BindShaderBundle (d3d_MeshFullbrightShader);
	else D_BindShaderBundle (d3d_MeshLightmapShader);

	D_BindVertexBuffer (0, set->PolyVerts, sizeof (aliaspolyvert_t), e->prevframe * sizeof (aliaspolyvert_t) * set->NumVerts);
	D_BindVertexBuffer (1, set->PolyVerts, sizeof (aliaspolyvert_t), e->currframe * sizeof (aliaspolyvert_t) * set->NumVerts);
	D_BindVertexBuffer (2, set->TexCoords, sizeof (aliastexcoord_t), 0);

	D_BindIndexBuffer (set->Indexes, DXGI_FORMAT_R16_UINT);
}


void R_TransformAliasModel (entity_t *e, dmdl_t *hdr, meshconstants_t *consts, QMATRIX *localmatrix)
{
	// set up the lerp transform
	float move[3], delta[3];
	float frontlerp = 1.0 - e->backlerp;

	// get current and previous frames
	daliasframe_t *currframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->currframe * hdr->framesize);
	daliasframe_t *prevframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->prevframe * hdr->framesize);

	// move should be the delta back to the previous frame * backlerp
	Vector3Subtract (delta, e->prevorigin, e->currorigin);

	// take these from the local matrix
	move[0] = Vector3Dot (delta, localmatrix->m4x4[0]);	// forward
	move[1] = Vector3Dot (delta, localmatrix->m4x4[1]);	// left
	move[2] = Vector3Dot (delta, localmatrix->m4x4[2]);	// up

	Vector3Add (move, move, prevframe->translate);

	Vector3Set (consts->move,
		e->backlerp * move[0] + frontlerp * currframe->translate[0],
		e->backlerp * move[1] + frontlerp * currframe->translate[1],
		e->backlerp * move[2] + frontlerp * currframe->translate[2]
	);

	Vector3Set (consts->frontv,
		frontlerp * currframe->scale[0],
		frontlerp * currframe->scale[1],
		frontlerp * currframe->scale[2]
	);

	Vector3Set (consts->backv,
		e->backlerp * prevframe->scale[0],
		e->backlerp * prevframe->scale[1],
		e->backlerp * prevframe->scale[2]
	);

	if (!(e->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)))
		consts->suitscale = 0;
	else consts->suitscale = POWERSUIT_SCALE;

	consts->backlerp = e->backlerp;
}


void R_LightAliasModel (entity_t *e, meshconstants_t *consts)
{
	// clear the dynamic lighting flag
	e->flags &= ~RF_DYNAMICLIGHT;

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
			VectorClear (consts->shadelight);

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
			VectorClear (consts->shadelight);

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
		float scale = 0.1 * sin (r_newrefdef.time * 7);
		int i;

		for (i = 0; i < 3; i++)
		{
			float min = consts->shadelight[i] * 0.8;

			consts->shadelight[i] += scale;

			if (consts->shadelight[i] < min)
				consts->shadelight[i] = min;
		}
	}

	R_SetEntityLighting (e, consts->shadelight, consts->shadevector);
}


static qboolean R_CullAliasModel (entity_t *e, QMATRIX *localmatrix)
{
	int i;
	model_t *mod = e->model;
	dmdl_t *hdr = (dmdl_t *) e->model->extradata;

	// the frames were fixed-up in R_PrepareAliasModel so we don't need to do so here
	daliasframe_t *currframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->currframe * hdr->framesize);
	daliasframe_t *prevframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->prevframe * hdr->framesize);

	// compute axially aligned mins and maxs
	if (currframe == prevframe)
	{
		for (i = 0; i < 3; i++)
		{
			mod->mins[i] = currframe->translate[i];
			mod->maxs[i] = mod->mins[i] + currframe->scale[i] * 255;
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			float currmins = currframe->translate[i];
			float currmaxs = currmins + currframe->scale[i] * 255;

			float prevmins = prevframe->translate[i];
			float prevmaxs = prevmins + prevframe->scale[i] * 255;

			if (currmins < prevmins) mod->mins[i] = currmins; else mod->mins[i] = prevmins;
			if (currmaxs > prevmaxs) mod->maxs[i] = currmaxs; else mod->maxs[i] = prevmaxs;
		}
	}

	// take a radius for dlight interaction tests
	mod->radius = Mod_RadiusFromBounds (mod->mins, mod->maxs);

	// and run the cull
	return R_CullForEntity (mod->mins, mod->maxs, localmatrix);
}


qboolean R_AliasLightInteraction (entity_t *e, model_t *mod, dlight_t *dl)
{
	// sphere/sphere intersection test
	return (Vector3Dist (e->currorigin, dl->origin) < (dl->intensity + mod->radius));
}


void R_AliasDlights (entity_t *e, model_t *mod, dmdl_t *hdr, QMATRIX *localMatrix)
{
	int	i;

	for (i = 0; i < r_newrefdef.num_dlights; i++)
	{
		dlight_t *dl = &r_newrefdef.dlights[i];

		// a dl that's been culled will have it's intensity set to 0
		if (!(dl->intensity > 0)) continue;

		if (R_AliasLightInteraction (e, mod, dl))
		{
			float origin[3];

			// copy off the origin, then move the light into entity local space
			Vector3Copy (origin, dl->origin);
			R_VectorInverseTransform (localMatrix, dl->origin, origin);

			// set up the light
			D_SetupDynamicLight (dl, e->flags);

			// set up the shaders
			D_BindShaderBundle (d3d_MeshDynamicShader);

			// and draw it
			GL_DrawAliasPolySet (mod);

			// restore the origin
			Vector3Copy (dl->origin, origin);
		}
	}

	// go to a new dlight frame for each push so that we don't carry over lights from the previous
	r_dlightframecount++;
}


void R_DrawAliasModel (entity_t *e, QMATRIX *localmatrix)
{
	model_t		*mod = e->model;
	dmdl_t		*hdr = (dmdl_t *) mod->extradata;

	// per-mesh cbuffer constants
	meshconstants_t consts;

	if (!(e->flags & RF_WEAPONMODEL))
	{
		if (R_CullAliasModel (e, localmatrix))
			return;
	}
	else
	{
		if (r_lefthand->value == 2)
			return;
	}

	if (e->flags & RF_TRANSLUCENT)
	{
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, R_GetRasterizerState (e->flags));
		R_UpdateAlpha (e->alpha);
	}
	else
	{
		D_SetRenderStates (d3d_BSNone, d3d_DSFullDepth, R_GetRasterizerState (e->flags));
		R_UpdateAlpha (1);
	}

	R_UpdateEntityConstants (localmatrix, NULL, e->flags);

	// set up our mesh constants
	R_LightAliasModel (e, &consts);
	R_TransformAliasModel (e, hdr, &consts, localmatrix);

	// and update to the cbuffer
	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_MeshConstants, 0, NULL, &consts, 0, 0);

	// set up the frame interpolation
	GL_SetupAliasFrameLerp (e, mod, &d3d_AliasBuffers[mod->bufferset]);

	// and draw it
	GL_DrawAliasPolySet (mod);

	// no dlights in the player setup screens
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;

	// no dlights in fullbright mode
	if (!r_worldmodel->lightdata || r_fullbright->value) return;

	// no dlights unless it's flagged for them
	if (!(e->flags & RF_DYNAMICLIGHT)) return;

	// add dynamic lighting to the entity
	R_AliasDlights (e, mod, hdr, localmatrix);
}


void R_PrepareAliasModel (entity_t *e, QMATRIX *localmatrix)
{
	model_t		*mod = e->model;
	dmdl_t		*hdr = (dmdl_t *) mod->extradata;

	// fix up the frames in advance of culling because it needs them
	if ((e->currframe >= hdr->num_frames) || (e->currframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such currframe %d\n", mod->name, e->currframe);
		e->currframe = 0;
		e->prevframe = 0;
	}

	if ((e->prevframe >= hdr->num_frames) || (e->prevframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such prevframe %d\n", mod->name, e->prevframe);
		e->currframe = 0;
		e->prevframe = 0;
	}

	// get the transform in local space so that we can correctly handle dlights
	R_MatrixIdentity (localmatrix);
	R_MatrixTranslate (localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotate (localmatrix, e->angles[0], e->angles[1], -e->angles[2]);
}

