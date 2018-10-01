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

extern model_t	*loadmodel;
extern int		modfilelen;


/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

void Mod_LoadAliasSTVerts (dmdl_t *pinmodel)
{
	int i;

	// load base s and t vertices
	// these are discarded after buffers are built so they just need to byte-swap in-place
	dstvert_t *stverts = (dstvert_t *) ((byte *) pinmodel + pinmodel->ofs_st);

	for (i = 0; i < pinmodel->num_st; i++)
	{
		stverts[i].s = LittleShort (stverts[i].s);
		stverts[i].t = LittleShort (stverts[i].t);
	}
}


void Mod_LoadAliasTriangles (dmdl_t *pinmodel)
{
	int i, j;

	// load triangle lists
	// these are discarded after buffers are built so they just need to byte-swap in-place
	dtriangle_t *triangles = (dtriangle_t *) ((byte *) pinmodel + pinmodel->ofs_tris);

	for (i = 0; i < pinmodel->num_tris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			triangles[i].index_xyz[j] = LittleShort (triangles[i].index_xyz[j]);
			triangles[i].index_st[j] = LittleShort (triangles[i].index_st[j]);
		}
	}
}


void Mod_LoadAliasFrames (dmdl_t *pinmodel, mmdl_t *pheader)
{
	int i, j;

	// alloc the frames
	pheader->frames = (maliasframe_t *) HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, pheader->num_frames * sizeof (maliasframe_t));

	// load the frames
	for (i = 0; i < pheader->num_frames; i++)
	{
		daliasframe_t *pinframe = (daliasframe_t *) ((byte *) pinmodel + pinmodel->ofs_frames + i * pinmodel->framesize);
		maliasframe_t *poutframe = &pheader->frames[i];

		for (j = 0; j < 3; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}

		// verts are all 8 bit, so no swapping needed; they are discarded after building so we don't need to save them off
	}
}


void Mod_LoadAliasSkins (dmdl_t *pinmodel, mmdl_t *pheader, model_t *mod)
{
	int i;

	// register all skins
	for (i = 0; i < pheader->num_skins; i++)
	{
		char *srcskin = (char *) pinmodel + pinmodel->ofs_skins + i * MAX_SKINNAME;

		pheader->skinnames[i] = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, strlen (srcskin) + 1);
		strcpy (pheader->skinnames[i], srcskin);

		mod->skins[i] = GL_FindImage (pheader->skinnames[i], it_skin);
	}
}


void Mod_LoadAliasHeader (dmdl_t *pinmodel, mmdl_t *pheader, model_t *mod)
{
	int i;

	// byte swap the header fields and sanity check
	for (i = 0; i < sizeof (dmdl_t) / 4; i++)
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


