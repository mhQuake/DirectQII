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

void Mod_LoadAliasSTVerts (dmdl_t *pinmodel, mmdl_t *pheader)
{
	int i;

	// load base s and t vertices (not used in gl version)
	dstvert_t *pinst = (dstvert_t *) ((byte *) pinmodel + pinmodel->ofs_st);
	mstvert_t *poutst = (mstvert_t *) ri.Hunk_Alloc (pinmodel->num_st * sizeof (mstvert_t));

	for (i = 0; i < pinmodel->num_st; i++)
	{
		poutst[i].s = (float) LittleShort (pinst[i].s) / (float) pinmodel->skinwidth;
		poutst[i].t = (float) LittleShort (pinst[i].t) / (float) pinmodel->skinheight;
	}

	pheader->stverts = poutst;
}


void Mod_LoadAliasTriangles (dmdl_t *pinmodel, mmdl_t *pheader)
{
	int i, j;

	// load triangle lists
	dtriangle_t *pintri = (dtriangle_t *) ((byte *) pinmodel + pinmodel->ofs_tris);
	mtriangle_t *pouttri = (mtriangle_t *) ri.Hunk_Alloc (pinmodel->num_tris * sizeof (mtriangle_t));

	for (i = 0; i < pinmodel->num_tris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[j]);
		}
	}

	pheader->triangles = pouttri;
}


void Mod_LoadAliasFrames (dmdl_t *pinmodel, mmdl_t *pheader)
{
	int i, j;
	mtrivertx_t *triverts = (mtrivertx_t *) ri.Hunk_Alloc (pinmodel->num_frames * pinmodel->num_xyz * sizeof (mtrivertx_t));

	// alloc the frames
	pheader->frames = (maliasframe_t *) ri.Hunk_Alloc (pheader->num_frames * sizeof (maliasframe_t));

	// load the frames
	for (i = 0; i < pheader->num_frames; i++, triverts += pinmodel->num_xyz)
	{
		daliasframe_t *pinframe = (daliasframe_t *) ((byte *) pinmodel + pinmodel->ofs_frames + i * pinmodel->framesize);
		maliasframe_t *poutframe = &pheader->frames[i];

		for (j = 0; j < 3; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}

		// verts are all 8 bit, so no swapping needed
		for (j = 0; j < pinmodel->num_xyz; j++)
		{
			triverts[j].v[0] = pinframe->verts[j].v[0];
			triverts[j].v[1] = pinframe->verts[j].v[1];
			triverts[j].v[2] = pinframe->verts[j].v[2];
			triverts[j].lightnormalindex = pinframe->verts[j].lightnormalindex;
		}

		// set the triverts pointer
		poutframe->triverts = triverts;
	}
}


void Mod_LoadAliasGLCommands (dmdl_t *pinmodel, mmdl_t *pheader)
{
	int i;

	// load the glcmds
	int *pincmd = (int *) ((byte *) pinmodel + pinmodel->ofs_glcmds);
	int *poutcmd = (int *) ri.Hunk_Alloc (sizeof (int) * pinmodel->num_glcmds);

	for (i = 0; i < pinmodel->num_glcmds; i++)
		poutcmd[i] = LittleLong (pincmd[i]);

	pheader->glcmds = poutcmd;
}


void Mod_LoadAliasSkins (dmdl_t *pinmodel, mmdl_t *pheader, model_t *mod)
{
	int i;

	// register all skins
	for (i = 0; i < pheader->num_skins; i++)
	{
		char *srcskin = (char *) pinmodel + pinmodel->ofs_skins + i * MAX_SKINNAME;

		pheader->skinnames[i] = ri.Hunk_Alloc (strlen (srcskin) + 1);
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
	mmdl_t *pheader = (mmdl_t *) ri.Hunk_Alloc (sizeof (mmdl_t));

	// load and set up the header
	Mod_LoadAliasHeader (pinmodel, pheader, mod);

	// load all the data (major fixme - this doesn't need to be kept around after the MDL is converted to vertex buffers)
	Mod_LoadAliasSTVerts (pinmodel, pheader);
	Mod_LoadAliasTriangles (pinmodel, pheader);
	Mod_LoadAliasFrames (pinmodel, pheader);
	Mod_LoadAliasGLCommands (pinmodel, pheader);
	Mod_LoadAliasSkins (pinmodel, pheader, mod);

	mod->type = mod_alias;

	// these will be replaced by the correct per-frame bboxes at runtime
	Vector3Set (mod->mins, -32, -32, -32);
	Vector3Set (mod->maxs, 32, 32, 32);
}


