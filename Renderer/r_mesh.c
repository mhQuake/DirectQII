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

qboolean VCache_ReorderIndices (char *name, unsigned short *outIndices, const unsigned short *indices, int nTriangles, int nVertices);
void VCache_Init (void);

// deduplication
typedef struct aliasmesh_s {
	short index_xyz;
	short index_st;
} aliasmesh_t;


typedef struct aliasbuffers_s {
	ID3D11Buffer *PolyVerts;
	ID3D11Buffer *TexCoords;
	ID3D11Buffer *Indexes;

	char Name[256];
	int registration_sequence;
} aliasbuffers_t;

__declspec(align(16)) typedef struct meshconstants_s {
	float shadelight[4];	// padded for cbuffer
	float shadevector[4];	// padded for cbuffer
	float move[4];			// padded for cbuffer
	float frontv[4];		// padded for cbuffer
	float backv[4];			// padded for cbuffer
	float suitscale;
	float backlerp;
	float junk[2];			// cbuffer padding
} meshconstants_t;


static aliasbuffers_t d3d_AliasBuffers[MAX_MOD_KNOWN];

static int d3d_MeshLightmapShader;
static int d3d_MeshDynamicShader;
static int d3d_MeshPowersuitShader;
static int d3d_MeshFullbrightShader;

static ID3D11Buffer *d3d_MeshConstants = NULL;

void R_InitMesh (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("PREVTRIVERTX", 0, DXGI_FORMAT_R8G8B8A8_UINT, 1, 0),
		VDECL ("CURRTRIVERTX", 0, DXGI_FORMAT_R8G8B8A8_UINT, 2, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 3, 0)
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


void D_CreateAliasPolyVerts (mmdl_t *hdr, dmdl_t *src, aliasbuffers_t *set, aliasmesh_t *dedupe)
{
	dtrivertx_t *polyverts = ri.Load_AllocMemory (hdr->num_verts * hdr->num_frames * sizeof (dtrivertx_t));
	int framenum, i;

	D3D11_BUFFER_DESC vbDesc = {
		hdr->num_verts * hdr->num_frames * sizeof (dtrivertx_t),
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = {polyverts, 0, 0};

	for (framenum = 0; framenum < hdr->num_frames; framenum++)
	{
		daliasframe_t *inframe = (daliasframe_t *) ((byte *) src + src->ofs_frames + framenum * src->framesize);

		for (i = 0; i < hdr->num_verts; i++, polyverts++)
		{
			dtrivertx_t *tv = &inframe->verts[dedupe[i].index_xyz];

			polyverts->v[0] = tv->v[0];
			polyverts->v[1] = tv->v[1];
			polyverts->v[2] = tv->v[2];
			polyverts->lightnormalindex = tv->lightnormalindex;
		}
	}

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->PolyVerts);
}


void D_CreateAliasTexCoords (mmdl_t *hdr, dmdl_t *src, aliasbuffers_t *set, aliasmesh_t *dedupe)
{
	float *texcoords = ri.Load_AllocMemory (hdr->num_verts * sizeof (float) * 2);
	int i;

	D3D11_BUFFER_DESC vbDesc = {
		hdr->num_verts * sizeof (float) * 2,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = {texcoords, 0, 0};

	// access source stverts
	dstvert_t *stverts = (dstvert_t *) ((byte *) src + src->ofs_st);

	for (i = 0; i < hdr->num_verts; i++, texcoords += 2)
	{
		dstvert_t *stvert = &stverts[dedupe[i].index_st];

		texcoords[0] = ((float) stvert->s + 0.5f) / hdr->skinwidth;
		texcoords[1] = ((float) stvert->t + 0.5f) / hdr->skinheight;
	}

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->TexCoords);
}


void D_CreateAliasIndexes (mmdl_t *hdr, aliasbuffers_t *set, unsigned short *indexes)
{
	D3D11_BUFFER_DESC ibDesc = {
		hdr->num_indexes * sizeof (unsigned short),
		D3D11_USAGE_IMMUTABLE,
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


void D_CreateAliasBufferSet (model_t *mod, mmdl_t *hdr, dmdl_t *src)
{
	aliasbuffers_t *set = &d3d_AliasBuffers[mod->bufferset];

	int i, j;

	aliasmesh_t *dedupe = (aliasmesh_t *) ri.Load_AllocMemory (hdr->num_verts * sizeof (aliasmesh_t));
	unsigned short *indexes = (unsigned short *) ri.Load_AllocMemory (hdr->num_indexes * sizeof (unsigned short));
	unsigned short *optimized = (unsigned short *) ri.Load_AllocMemory (hdr->num_indexes * sizeof (unsigned short));

	int num_verts = 0;
	int num_indexes = 0;

	// set up source data
	dtriangle_t *triangles = (dtriangle_t *) ((byte *) src + src->ofs_tris);

	for (i = 0; i < hdr->num_tris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			int v;

			for (v = 0; v < num_verts; v++)
			{
				if (triangles[i].index_xyz[j] != dedupe[v].index_xyz) continue;
				if (triangles[i].index_st[j] != dedupe[v].index_st) continue;

				// exists; emit an index for it
				indexes[num_indexes] = v;

				// no need to check any more
				break;
			}

			if (v == num_verts)
			{
				// doesn't exist; emit a new index...
				indexes[num_indexes] = num_verts;

				// ...and a new vert
				dedupe[num_verts].index_xyz = triangles[i].index_xyz[j];
				dedupe[num_verts].index_st = triangles[i].index_st[j];

				// go to the next vert
				num_verts++;
			}

			// go to the next index
			num_indexes++;
		}
	}

	// ri.Con_Printf (PRINT_ALL, "%s has %i verts from %i\n", mod->name, num_verts, hdr->num_verts);

	// store off the counts
	hdr->num_verts = num_verts;		// this is expected to be significantly lower (one-third or less)
	hdr->num_indexes = num_indexes; // this is expected to be unchanged (it's an error if it is

	// optimize index order for vertex cache
	if (VCache_ReorderIndices (mod->name, optimized, indexes, hdr->num_tris, hdr->num_verts))
	{
		// if it optimized we must re-order and remap the indices so that the vertex buffer can be accessed linearly
		// https://tomforsyth1000.github.io/papers/fast_vert_cache_opt.html
		aliasmesh_t *newverts = (aliasmesh_t *) ri.Load_AllocMemory (hdr->num_verts * sizeof (aliasmesh_t));
		int *inBuffer = (int *) ri.Load_AllocMemory (hdr->num_verts * sizeof (int)); // this can't be an unsigned short because we use -1 to indicate that it's not yet in the buffer
		int vertnum = 0;

		// because 0 is a valid index we must use -1 to indicate a vertex that's not yet in it's final, optimized, buffer position
		for (i = 0; i < hdr->num_verts; i++)
			inBuffer[i] = -1;

		// build the remap table
		for (i = 0; i < hdr->num_indexes; i++)
		{
			// is the referenced vertex in the buffer yet???
			if (inBuffer[optimized[i]] == -1)
			{
				// this is now an extra buffer entry
				newverts[vertnum].index_xyz = dedupe[optimized[i]].index_xyz;
				newverts[vertnum].index_st = dedupe[optimized[i]].index_st;

				// mark it as in the buffer and go to the next vert
				inBuffer[optimized[i]] = vertnum;
				vertnum++;
			}

			// add it to the indexes remap
			indexes[i] = inBuffer[optimized[i]];
		}

		// finally swap the pointers so that the loading routines will use the remapped vertices and indices
		dedupe = newverts;
		optimized = indexes;
	}

	// and build them all
	D_CreateAliasPolyVerts (hdr, src, set, dedupe);
	D_CreateAliasTexCoords (hdr, src, set, dedupe);
	D_CreateAliasIndexes (hdr, set, optimized);

	// release memory used for loading and building
	ri.Load_FreeMemory ();
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


void D_RegisterAliasBuffers (model_t *mod)
{
	// see do we already have it
	if ((mod->bufferset = D_FindAliasBuffers (mod)) != -1) return;
	ri.Sys_Error (ERR_DROP, "D_RegisterAliasBuffers : buffer set for %s was not created", mod->name);
}


void D_MakeAliasBuffers (model_t *mod, dmdl_t *src)
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
		D_CreateAliasBufferSet (mod, mod->md2header, src);

		// and done
		return;
	}

	ri.Sys_Error (ERR_DROP, "D_MakeAliasBuffers : not enough free buffers!");
}


image_t *R_SelectAliasTexture (entity_t *e, model_t *mod)
{
	// nasty shit
	image_t	*skin = NULL;

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


void R_SelectAliasShader (int eflags)
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


void R_DrawAliasPolySet (model_t *mod)
{
	aliasbuffers_t *set = &d3d_AliasBuffers[mod->bufferset];
	mmdl_t *hdr = mod->md2header;

	d3d_Context->lpVtbl->DrawIndexed (d3d_Context, hdr->num_indexes, 0, 0);
}


void R_SetupAliasFrameLerp (entity_t *e, model_t *mod, aliasbuffers_t *set)
{
	// sets up stuff that's going to be valid for both the main pass and the dynamic lighting pass(es)
	mmdl_t *hdr = mod->md2header;

	R_BindTexture (R_SelectAliasTexture (e, mod)->SRV);

	D_BindVertexBuffer (1, set->PolyVerts, sizeof (dtrivertx_t), e->prevframe * sizeof (dtrivertx_t) * hdr->num_verts);
	D_BindVertexBuffer (2, set->PolyVerts, sizeof (dtrivertx_t), e->currframe * sizeof (dtrivertx_t) * hdr->num_verts);
	D_BindVertexBuffer (3, set->TexCoords, sizeof (float) * 2, 0);

	D_BindIndexBuffer (set->Indexes, DXGI_FORMAT_R16_UINT);
}


void R_TransformAliasModel (entity_t *e, mmdl_t *hdr, meshconstants_t *consts, QMATRIX *localmatrix)
{
	// set up the lerp transform
	float move[3], delta[3];
	float frontlerp = 1.0 - e->backlerp;

	// get current and previous frames
	maliasframe_t *currframe = &hdr->frames[e->currframe];
	maliasframe_t *prevframe = &hdr->frames[e->prevframe];

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


void R_LightAliasModel (entity_t *e, meshconstants_t *consts, QMATRIX *localmatrix)
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
}


static qboolean R_CullAliasModel (entity_t *e, QMATRIX *localmatrix)
{
	int i;
	model_t *mod = e->model;
	mmdl_t *hdr = mod->md2header;

	// the frames were fixed-up in R_PrepareAliasModel so we don't need to do so here
	maliasframe_t *currframe = &hdr->frames[e->currframe];
	maliasframe_t *prevframe = &hdr->frames[e->prevframe];

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
	return (Vector3Dist (e->currorigin, dl->origin) < (dl->radius + mod->radius));
}


void R_AliasDlights (entity_t *e, model_t *mod, mmdl_t *hdr, QMATRIX *localMatrix)
{
	int	i;

	for (i = 0; i < r_newrefdef.num_dlights; i++)
	{
		dlight_t *dl = &r_newrefdef.dlights[i];

		// a dl that's been culled will have it's intensity set to 0
		if (!(dl->radius > 0)) continue;

		if (R_AliasLightInteraction (e, mod, dl))
		{
			// move the light into entity local space
			float transformedorigin[3];
			R_VectorInverseTransform (localMatrix, transformedorigin, dl->origin);

			// set up the light
			D_SetupDynamicLight (dl, transformedorigin, e->flags);

			// set up the shaders
			D_BindShaderBundle (d3d_MeshDynamicShader);

			// and draw it
			R_DrawAliasPolySet (mod);
		}
	}

	// go to a new dlight frame for each push so that we don't carry over lights from the previous
	r_dlightframecount++;
}


void R_DrawAliasModel (entity_t *e, QMATRIX *localmatrix)
{
	model_t		*mod = e->model;
	mmdl_t		*hdr = mod->md2header;

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

	// cbuffers/states
	R_PrepareEntityForRendering (localmatrix, NULL, e->alpha, e->flags);

	// set up our mesh constants
	R_LightAliasModel (e, &consts, localmatrix);
	R_TransformAliasModel (e, hdr, &consts, localmatrix);

	// and update to the cbuffer
	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_MeshConstants, 0, NULL, &consts, 0, 0);

	// set up the frame interpolation
	R_SetupAliasFrameLerp (e, mod, &d3d_AliasBuffers[mod->bufferset]);

	// select the correct shader to use for the main pass
	R_SelectAliasShader (e->flags);

	// and draw it
	R_DrawAliasPolySet (mod);

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
	mmdl_t		*hdr = mod->md2header;

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

