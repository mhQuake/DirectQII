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

model_t	*loadmodel;
int		modfilelen;

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS / 8];

model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// the inline * models from the current map are kept seperate
model_t	mod_inline[MAX_MOD_KNOWN];

int		r_registration_sequence;


/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	cplane_t	*plane;

	if (!model || !model->nodes)
		ri.Sys_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents != -1)
			return (mleaf_t *) node;
		plane = node->plane;
		d = Vector3Dot (p, plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS / 8];
	int		c;
	byte	*out;
	int		row;

	row = (model->vis->numclusters + 7) >> 3;
	out = decompressed;

	if (!in)
	{
		// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_DecompressVis ((byte *) model->vis + model->vis->bitofs[cluster][DVIS_PVS],
		model);
}


//===============================================================================


/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof (mod_novis));
}



/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	unsigned *buf;
	int		i;

	if (!name[0])
		ri.Sys_Error (ERR_DROP, "Mod_ForName: NULL name");

	// inline models are grabbed only from worldmodel
	if (name[0] == '*')
	{
		i = atoi (name + 1);

		if (i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels)
			ri.Sys_Error (ERR_DROP, "bad inline model number");

		return &mod_inline[i];
	}

	// search the currently loaded models
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (!strcmp (mod->name, name))
			return mod;
	}

	// find a free model slot spot
	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
			break;	// free spot
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			ri.Sys_Error (ERR_DROP, "mod_numknown == MAX_MOD_KNOWN");

		mod_numknown++;
	}

	strcpy (mod->name, name);

	// load the file
	modfilelen = ri.FS_LoadFile (mod->name, &buf);

	if (!buf)
	{
		if (crash)
			ri.Sys_Error (ERR_DROP, "Mod_ForName: %s not found", mod->name);

		memset (mod->name, 0, sizeof (mod->name));
		return NULL;
	}

	loadmodel = mod;

	// this should never happen unless we fail to call Mod_Free properly
	if (loadmodel->hHeap)
	{
		HeapDestroy (loadmodel->hHeap);
		loadmodel->hHeap = NULL;
	}

	// create the memory heap used by this model
	loadmodel->hHeap = HeapCreate (0, 0, 0);

	// fill it in - call the apropriate loader
	switch (LittleLong (*(unsigned *) buf))
	{
	case IDALIASHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;

	case IDBSPHEADER:
		Mod_LoadBrushModel (mod, buf);
		break;

	default:
		ri.Sys_Error (ERR_DROP, "Mod_NumForName: unknown fileid for %s", mod->name);
		break;
	}

	ri.FS_FreeFile (buf);

	return mod;
}


/*
=================
Mod_RadiusFromBounds
=================
*/
float Mod_RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs (mins[i]) > fabs (maxs[i]) ? fabs (mins[i]) : fabs (maxs[i]);

	return Vector3Length (corner);
}


int Mod_SignbitsForPlane (cplane_t *out)
{
	// for fast box on planeside test
	int j, bits = 0;

	for (j = 0; j < 3; j++)
		if (out->normal[j] < 0)
			bits |= 1 << j;

	return bits;
}


//=============================================================================

/*
=====================
R_BeginRegistration

Specifies the model that will be used as the world
=====================
*/
void R_BeginRegistration (char *model)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;

	r_registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs

	Com_sprintf (fullname, sizeof (fullname), "maps/%s.bsp", model);

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = ri.Cvar_Get ("flushmap", "0", 0);

	if (strcmp (mod_known[0].name, fullname) || flushmap->value)
		Mod_Free (&mod_known[0]);

	r_worldmodel = Mod_ForName (fullname, true);
	r_viewcluster = -1;
}


/*
=====================
R_RegisterModel

=====================
*/
struct model_s *R_RegisterModel (char *name)
{
	int		i;
	model_t	*mod = Mod_ForName (name, false);

	if (mod)
	{
		mod->registration_sequence = r_registration_sequence;

		// register any images used by the models
		if (mod->type == mod_sprite)
		{
			dsprite_t *sprout = mod->sprheader;

			for (i = 0; i < sprout->numframes; i++)
				mod->skins[i] = GL_FindImage (sprout->frames[i].name, it_sprite);
		}
		else if (mod->type == mod_alias)
		{
			mmdl_t *pheader = mod->md2header;

			for (i = 0; i < pheader->num_skins; i++)
				mod->skins[i] = GL_FindImage (pheader->skinnames[i], it_skin);

			mod->numframes = pheader->num_frames;

			// register vertex and index buffers
			D_RegisterAliasBuffers (mod);
		}
		else if (mod->type == mod_brush)
		{
			for (i = 0; i < mod->numtexinfo; i++)
				mod->texinfo[i].image->registration_sequence = r_registration_sequence;
		}
	}

	return mod;
}


/*
=====================
R_EndRegistration

=====================
*/
void R_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
			continue;

		if (mod->registration_sequence != r_registration_sequence)
		{
			// don't need this model
			Mod_Free (mod);
		}
	}

	// free any GPU objects not touched in this reg sequence
	R_FreeUnusedAliasBuffers ();
	R_FreeUnusedSpriteBuffers ();
	R_FreeUnusedImages ();
}


//=============================================================================


/*
================
Mod_Free
================
*/
void Mod_Free (model_t *mod)
{
	if (mod->hHeap)
	{
		HeapDestroy (mod->hHeap);
		mod->hHeap = NULL;
	}

	memset (mod, 0, sizeof (*mod));
}


/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll (void)
{
	int		i;

	for (i = 0; i < mod_numknown; i++)
		Mod_Free (&mod_known[i]);

	memset (mod_known, 0, sizeof (mod_known));
}


