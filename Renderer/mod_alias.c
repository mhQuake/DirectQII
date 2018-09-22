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

void Mod_LoadAliasSTVerts (dmdl_t *pinmodel, dmdl_t *pheader)
{
	int i;

	// load base s and t vertices (not used in gl version)
	dstvert_t *pinst = (dstvert_t *) ((byte *) pinmodel + pheader->ofs_st);
	dstvert_t *poutst = (dstvert_t *) ((byte *) pheader + pheader->ofs_st);

	for (i = 0; i < pheader->num_st; i++)
	{
		poutst[i].s = LittleShort (pinst[i].s);
		poutst[i].t = LittleShort (pinst[i].t);
	}
}


void Mod_LoadAliasTriangles (dmdl_t *pinmodel, dmdl_t *pheader)
{
	int i, j;

	// load triangle lists
	dtriangle_t *pintri = (dtriangle_t *) ((byte *) pinmodel + pheader->ofs_tris);
	dtriangle_t *pouttri = (dtriangle_t *) ((byte *) pheader + pheader->ofs_tris);

	for (i = 0; i < pheader->num_tris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[j]);
			pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[j]);
		}
	}
}


void Mod_LoadAliasFrames (dmdl_t *pinmodel, dmdl_t *pheader)
{
	int i, j;

	// load the frames
	for (i = 0; i < pheader->num_frames; i++)
	{
		daliasframe_t *pinframe = (daliasframe_t *) ((byte *) pinmodel + pheader->ofs_frames + i * pheader->framesize);
		daliasframe_t *poutframe = (daliasframe_t *) ((byte *) pheader + pheader->ofs_frames + i * pheader->framesize);

		memcpy (poutframe->name, pinframe->name, sizeof (poutframe->name));

		for (j = 0; j < 3; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}

		// verts are all 8 bit, so no swapping needed
		memcpy (poutframe->verts, pinframe->verts, pheader->num_xyz * sizeof (dtrivertx_t));
	}
}


void Mod_LoadAliasGLCommands (dmdl_t *pinmodel, dmdl_t *pheader)
{
	int i;

	// load the glcmds
	int *pincmd = (int *) ((byte *) pinmodel + pheader->ofs_glcmds);
	int *poutcmd = (int *) ((byte *) pheader + pheader->ofs_glcmds);

	for (i = 0; i < pheader->num_glcmds; i++)
		poutcmd[i] = LittleLong (pincmd[i]);
}


/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int		i;
	dmdl_t	*pheader;

	dmdl_t *pinmodel = (dmdl_t *) buffer;
	int version = LittleLong (pinmodel->version);

	if (version != ALIAS_VERSION)
		ri.Sys_Error (ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);

	pheader = (dmdl_t *) ri.Hunk_Alloc (LittleLong (pinmodel->ofs_end));

	// byte swap the header fields and sanity check
	for (i = 0; i < sizeof (dmdl_t) / 4; i++)
		((int *) pheader)[i] = LittleLong (((int *) buffer)[i]);

	if (pheader->skinheight > MAX_LBM_HEIGHT) ri.Sys_Error (ERR_DROP, "model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);
	if (pheader->num_xyz <= 0) ri.Sys_Error (ERR_DROP, "model %s has no vertices", mod->name);
	if (pheader->num_xyz > MAX_VERTS) ri.Sys_Error (ERR_DROP, "model %s has too many vertices", mod->name);
	if (pheader->num_st <= 0) ri.Sys_Error (ERR_DROP, "model %s has no st vertices", mod->name);
	if (pheader->num_tris <= 0) ri.Sys_Error (ERR_DROP, "model %s has no triangles", mod->name);
	if (pheader->num_frames <= 0) ri.Sys_Error (ERR_DROP, "model %s has no frames", mod->name);

	// load all the data (major fixme - this doesn't need to be kept around after the MDL is converted to vertex buffers)
	Mod_LoadAliasSTVerts (pinmodel, pheader);
	Mod_LoadAliasTriangles (pinmodel, pheader);
	Mod_LoadAliasFrames (pinmodel, pheader);
	Mod_LoadAliasGLCommands (pinmodel, pheader);

	mod->type = mod_alias;

	// register all skins
	memcpy ((char *) pheader + pheader->ofs_skins, (char *) pinmodel + pheader->ofs_skins, pheader->num_skins * MAX_SKINNAME);

	for (i = 0; i < pheader->num_skins; i++)
		mod->skins[i] = GL_FindImage ((char *) pheader + pheader->ofs_skins + i * MAX_SKINNAME, it_skin);

	Vector3Set (mod->mins, -32, -32, -32);
	Vector3Set (mod->maxs, 32, 32, 32);
}


