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
// models.c -- model loading and caching

#include "r_local.h"
#include "mesh.h"


extern model_t	*loadmodel;
extern int		modfilelen;


/*
==============================================================================

VERTEX BUFFER BUILDING

==============================================================================
*/

void D_CreateAliasPolyVerts (mmdl_t *hdr, dmdl_t *src, aliasbuffers_t *set, aliasmesh_t *dedupe)
{
	meshpolyvert_t *polyverts = ri.Load_AllocMemory (hdr->num_verts * hdr->num_frames * sizeof (meshpolyvert_t));

	D3D11_BUFFER_DESC vbDesc = {
		hdr->num_verts * hdr->num_frames * sizeof (meshpolyvert_t),
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = { polyverts, 0, 0 };

	for (int framenum = 0; framenum < hdr->num_frames; framenum++)
	{
		daliasframe_t *inframe = (daliasframe_t *) ((byte *) src + src->ofs_frames + framenum * src->framesize);

		for (int i = 0; i < hdr->num_verts; i++, polyverts++)
		{
			dtrivertx_t *tv = &inframe->verts[dedupe[i].index_xyz];

			polyverts->position[0] = tv->v[0] * inframe->scale[0] + inframe->translate[0];
			polyverts->position[1] = tv->v[1] * inframe->scale[1] + inframe->translate[1];
			polyverts->position[2] = tv->v[2] * inframe->scale[2] + inframe->translate[2];

			R_GetVertexNormal (polyverts->normal, tv->lightnormalindex);
		}
	}

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->PolyVerts);

	// per-frame offset into the buffer
	set->framevertexstride = hdr->num_verts * sizeof (meshpolyvert_t);
}


void D_CreateAliasTexCoords (mmdl_t *hdr, dmdl_t *src, aliasbuffers_t *set, aliasmesh_t *dedupe)
{
	float *texcoords = ri.Load_AllocMemory (hdr->num_verts * sizeof (float) * 2);

	D3D11_BUFFER_DESC vbDesc = {
		hdr->num_verts * sizeof (float) * 2,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = { texcoords, 0, 0 };

	// access source stverts
	dstvert_t *stverts = (dstvert_t *) ((byte *) src + src->ofs_st);

	for (int i = 0; i < hdr->num_verts; i++, texcoords += 2)
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
	D3D11_SUBRESOURCE_DATA srd = { indexes, 0, 0 };

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &set->Indexes);

	// copy off number of indexes to the set so that we can draw independent of model format
	set->numindexes = hdr->num_indexes;
}


void D_CreateAliasBufferSet (model_t *mod, mmdl_t *hdr, dmdl_t *src)
{
	aliasbuffers_t *set = R_GetBufferSetForIndex (mod->bufferset);

	aliasmesh_t *dedupe = (aliasmesh_t *) ri.Load_AllocMemory (hdr->num_verts * sizeof (aliasmesh_t));
	unsigned short *indexes = (unsigned short *) ri.Load_AllocMemory (hdr->num_indexes * sizeof (unsigned short));
	unsigned short *optimized = (unsigned short *) ri.Load_AllocMemory (hdr->num_indexes * sizeof (unsigned short));

	int num_verts = 0;
	int num_indexes = 0;

	// set up source data
	dtriangle_t *triangles = (dtriangle_t *) ((byte *) src + src->ofs_tris);

	for (int i = 0; i < hdr->num_tris; i++)
	{
		for (int j = 0; j < 3; j++)
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
		for (int i = 0; i < hdr->num_verts; i++)
			inBuffer[i] = -1;

		// build the remap table
		for (int i = 0; i < hdr->num_indexes; i++)
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


void D_MakeAliasBuffers (model_t *mod, dmdl_t *src)
{
	// see do we already have it
	if ((mod->bufferset = D_FindAliasBuffers (mod)) != -1) return;

	if ((mod->bufferset = D_GetFreeBufferSet ()) != -1)
	{
		aliasbuffers_t *set = R_GetBufferSetForIndex (mod->bufferset);

		// cache the name so that we'll find it next time too
		strcpy (set->Name, mod->name);

		// mark it as active
		set->registration_sequence = r_registration_sequence;

		// now build everything from the model data
		D_CreateAliasBufferSet (mod, mod->md2header, src);

		// and done
		return;
	}

	ri.Sys_Error (ERR_DROP, "D_MakeAliasBuffers : not enough free buffers!");
}


/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

void Mod_LoadAliasSTVerts (dmdl_t *pinmodel)
{
	// load base s and t vertices
	// these are discarded after buffers are built so they just need to byte-swap in-place
	dstvert_t *stverts = (dstvert_t *) ((byte *) pinmodel + pinmodel->ofs_st);

	for (int i = 0; i < pinmodel->num_st; i++)
	{
		stverts[i].s = LittleShort (stverts[i].s);
		stverts[i].t = LittleShort (stverts[i].t);
	}
}


void Mod_LoadAliasTriangles (dmdl_t *pinmodel)
{
	// load triangle lists
	// these are discarded after buffers are built so they just need to byte-swap in-place
	dtriangle_t *triangles = (dtriangle_t *) ((byte *) pinmodel + pinmodel->ofs_tris);

	for (int i = 0; i < pinmodel->num_tris; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			triangles[i].index_xyz[j] = LittleShort (triangles[i].index_xyz[j]);
			triangles[i].index_st[j] = LittleShort (triangles[i].index_st[j]);
		}
	}
}


void Mod_LoadAliasFrames (dmdl_t *pinmodel, mmdl_t *pheader)
{
	// alloc the frames
	pheader->frames = (maliasframe_t *) HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, pheader->num_frames * sizeof (maliasframe_t));

	// load the frames
	for (int i = 0; i < pheader->num_frames; i++)
	{
		daliasframe_t *pinframe = (daliasframe_t *) ((byte *) pinmodel + pinmodel->ofs_frames + i * pinmodel->framesize);
		maliasframe_t *poutframe = &pheader->frames[i];

		for (int j = 0; j < 3; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}

		// verts are all 8 bit, so no swapping needed; they are discarded after building so we don't need to save them off
	}
}


void Mod_LoadAliasSkins (dmdl_t *pinmodel, mmdl_t *pheader, model_t *mod)
{
	// register all skins
	for (int i = 0; i < pheader->num_skins; i++)
	{
		char *srcskin = (char *) pinmodel + pinmodel->ofs_skins + i * MAX_SKINNAME;

		pheader->skinnames[i] = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, strlen (srcskin) + 1);
		strcpy (pheader->skinnames[i], srcskin);

		mod->skins[i] = GL_FindImage (pheader->skinnames[i], it_skin);
	}
}


void Mod_LoadAliasHeader (dmdl_t *pinmodel, mmdl_t *pheader, model_t *mod)
{
	// byte swap the header fields and sanity check
	for (int i = 0; i < sizeof (dmdl_t) / 4; i++)
		((int *) pinmodel)[i] = LittleLong (((int *) pinmodel)[i]);

	// validate
	if (pinmodel->version != ALIAS_VERSION) ri.Sys_Error (ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, pinmodel->version, ALIAS_VERSION);
	if (pinmodel->skinheight > MAX_LBM_HEIGHT) ri.Sys_Error (ERR_DROP, "model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);
	if (pinmodel->num_xyz <= 0) ri.Sys_Error (ERR_DROP, "model %s has no vertices", mod->name);
	if (pinmodel->num_xyz > MAX_VERTS) ri.Sys_Error (ERR_DROP, "model %s has too many vertices", mod->name);
	if (pinmodel->num_st <= 0) ri.Sys_Error (ERR_DROP, "model %s has no st vertices", mod->name);
	if (pinmodel->num_tris <= 0) ri.Sys_Error (ERR_DROP, "model %s has no triangles", mod->name);
	if (pinmodel->num_frames <= 0) ri.Sys_Error (ERR_DROP, "model %s has no frames", mod->name);

	// copy over the data
	pheader->num_frames = pinmodel->num_frames;
	pheader->num_skins = pinmodel->num_skins;
	pheader->num_tris = pinmodel->num_tris;
	pheader->num_verts = pinmodel->num_tris * 3;
	pheader->num_indexes = pinmodel->num_tris * 3;
	pheader->skinheight = pinmodel->skinheight;
	pheader->skinwidth = pinmodel->skinwidth;
}


/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	dmdl_t *pinmodel = (dmdl_t *) buffer;
	mmdl_t *pheader = (mmdl_t *) HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, sizeof (mmdl_t));

	mod->sprheader = NULL;
	mod->md2header = pheader;

	// load and set up the header
	Mod_LoadAliasHeader (pinmodel, pheader, mod);

	// load all the data (major fixme - this doesn't need to be kept around after the MDL is converted to vertex buffers)
	Mod_LoadAliasSTVerts (pinmodel);
	Mod_LoadAliasTriangles (pinmodel);
	Mod_LoadAliasFrames (pinmodel, pheader);
	Mod_LoadAliasSkins (pinmodel, pheader, mod);

	mod->type = mod_alias;

	// these will be replaced by the correct per-frame bboxes at runtime
	Vector3Set (mod->mins, -32, -32, -32);
	Vector3Set (mod->maxs, 32, 32, 32);

	// create vertex and index buffers
	D_MakeAliasBuffers (mod, pinmodel);
}


