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
===============================================================================

BRUSHMODEL LOADING

===============================================================================
*/

extern model_t	mod_known[MAX_MOD_KNOWN];
extern int		mod_numknown;

// the inline * models from the current map are kept seperate
extern model_t	mod_inline[MAX_MOD_KNOWN];

byte	*mod_base;

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l, dbsp_t *bsp)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}

	loadmodel->lightdata = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, l->filelen);
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l, dbsp_t *bsp)
{
	int		i;

	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}

	loadmodel->vis = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, l->filelen);
	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);

	loadmodel->vis->numclusters = LittleLong (loadmodel->vis->numclusters);

	for (i = 0; i < loadmodel->vis->numclusters; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong (loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong (loadmodel->vis->bitofs[i][1]);
	}
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l, dbsp_t *bsp)
{
	dvertex_t	*in = (dvertex_t *) (mod_base + l->fileofs);
	int			i, count;

	if (l->filelen % sizeof (dvertex_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadVertexes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (dvertex_t);
	bsp->vertexes = in;

	for (i = 0; i < count; i++, in++)
	{
		in->point[0] = LittleFloat (in->point[0]);
		in->point[1] = LittleFloat (in->point[1]);
		in->point[2] = LittleFloat (in->point[2]);
	}
}


/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l, dbsp_t *bsp)
{
	dmodel_t	*in = (dmodel_t *) (mod_base + l->fileofs);
	mmodel_t	*out;
	int			i, j, count;

	if (l->filelen % sizeof (dmodel_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (dmodel_t);
	out = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, count * sizeof (mmodel_t));

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}

		out->radius = Mod_RadiusFromBounds (out->mins, out->maxs);
		out->headnode = LittleLong (in->headnode);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l, dbsp_t *bsp)
{
	dedge_t *in = (dedge_t *) (mod_base + l->fileofs);
	int 	i, count;

	if (l->filelen % sizeof (dedge_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadEdges: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (dedge_t);
	bsp->edges = in;

	for (i = 0; i < count; i++, in++)
	{
		in->v[0] = (unsigned short) LittleShort (in->v[0]);
		in->v[1] = (unsigned short) LittleShort (in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l, dbsp_t *bsp)
{
	texinfo_t *in= (texinfo_t *) (mod_base + l->fileofs);
	mtexinfo_t *out, *step;
	int 	i, j, count;
	char	name[MAX_QPATH];
	int		next;

	if (l->filelen % sizeof (texinfo_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (texinfo_t);
	out = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, count * sizeof (mtexinfo_t));

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);

		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);

		if (next > 0)
			out->next = loadmodel->texinfo + next;
		else out->next = NULL;

		Com_sprintf (name, sizeof (name), "textures/%s.wal", in->texture);

		if ((out->image = GL_FindImage (name, it_wall)) == NULL)
		{
			ri.Con_Printf (PRINT_ALL, "Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	// count animation frames
	for (i = 0; i < count; i++)
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;

		for (step = out->next; step && step != out; step = step->next)
			out->numframes++;
	}
}


/*
================
Mod_CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void Mod_CalcSurfaceExtents (msurface_t *s, dbsp_t *bsp)
{
	int		i;
	float	mins[2] = {999999, 999999};
	float	maxs[2] = {-999999, -999999};
	mtexinfo_t	*tex = s->texinfo;

	s->mins[0] = s->mins[1] = s->mins[2] = 999999;
	s->maxs[0] = s->maxs[1] = s->maxs[2] = -999999;

	for (i = 0; i < s->numedges; i++)
	{
		int j, e = bsp->surfedges[s->firstedge + i];
		dvertex_t *v;

		if (e >= 0)
			v = &bsp->vertexes[bsp->edges[e].v[0]];
		else v = &bsp->vertexes[bsp->edges[-e].v[1]];

		for (j = 0; j < 3; j++)
		{
			if (v->point[j] < s->mins[j]) s->mins[j] = v->point[j];
			if (v->point[j] > s->maxs[j]) s->maxs[j] = v->point[j];
		}

		for (j = 0; j < 2; j++)
		{
			float val = Vector3Dot (v->point, tex->vecs[j]) + tex->vecs[j][3];

			if (val < mins[j]) mins[j] = val;
			if (val > maxs[j]) maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++)
	{
		int bmins = floor (mins[i] / 16);
		int bmaxs = ceil (maxs[i] / 16);

		s->texturemins[i] = bmins * 16;
		s->extents[i] = (bmaxs - bmins) * 16;
	}
}


void R_BeginBuildingLightmaps (model_t *m);
void R_CreateSurfaceLightmap (msurface_t *surf);
void R_EndBuildingLightmaps (void);

void R_BeginBuildingSurfaces (model_t *mod);
void R_RegisterSurface (msurface_t *surf);
void R_EndBuildingSurfaces (model_t *mod, dbsp_t *bsp);


/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l, dbsp_t *bsp)
{
	dface_t		*in = (dface_t *) (mod_base + l->fileofs);
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int			ti;

	if (l->filelen % sizeof (dface_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (dface_t);
	out = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, count * sizeof (msurface_t));

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	R_BeginBuildingSurfaces (loadmodel);
	R_BeginBuildingLightmaps (loadmodel);

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong (in->firstedge);
		out->numedges = LittleShort (in->numedges);
		out->flags = 0;

		planenum = (unsigned short) LittleShort (in->planenum);
		side = LittleShort (in->side);

		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = loadmodel->planes + planenum;
		ti = (unsigned short) LittleShort (in->texinfo);

		if (ti >= loadmodel->numtexinfo)
			ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: bad texinfo number");

		out->texinfo = loadmodel->texinfo + ti;

		Mod_CalcSurfaceExtents (out, bsp);

		// lighting info
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];

		if ((i = LittleLong (in->lightofs)) == -1)
			out->samples = NULL;
		else out->samples = loadmodel->lightdata + i;

		// set the drawing flags
		if (out->texinfo->flags & SURF_WARP)
		{
			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
		}

		// create lightmaps and polygons
		if (!(out->texinfo->flags & SURF_NOLIGHTMAP))
			R_CreateSurfaceLightmap (out);

		R_RegisterSurface (out);
	}

	R_EndBuildingLightmaps ();
	R_EndBuildingSurfaces (loadmodel, bsp);
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;

	if (node->contents != -1)
		return;

	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l, dbsp_t *bsp)
{
	int			i, j, count, p;
	dnode_t		*in = (dnode_t *) (mod_base + l->fileofs);
	mnode_t 	*out;

	if (l->filelen % sizeof (dnode_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadNodes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (dnode_t);
	out = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, count * sizeof (mnode_t));

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong (in->planenum);
		out->plane = loadmodel->planes + p;

		out->surfaces = loadmodel->surfaces + (unsigned short) LittleShort (in->firstface);
		out->numsurfaces = (unsigned short) LittleShort (in->numfaces);
		out->contents = -1;	// differentiate from leafs

		for (j = 0; j < 2; j++)
		{
			p = LittleLong (in->children[j]);

			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else out->children[j] = (mnode_t *) (loadmodel->leafs + (-1 - p));
		}
	}

	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l, dbsp_t *bsp)
{
	dleaf_t 	*in = (dleaf_t *) (mod_base + l->fileofs);
	mleaf_t 	*out;
	int			i, j, count, p;

	if (l->filelen % sizeof (dleaf_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadLeafs: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (dleaf_t);
	out = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, count * sizeof (mleaf_t));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->mins[j] = LittleShort (in->mins[j]);
			out->maxs[j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong (in->contents);
		out->contents = p;

		out->cluster = LittleShort (in->cluster);
		out->area = LittleShort (in->area);

		out->firstmarksurface = loadmodel->marksurfaces + (unsigned short) LittleShort (in->firstleafface);
		out->nummarksurfaces = (unsigned short) LittleShort (in->numleaffaces);
	}
}


/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l, dbsp_t *bsp)
{
	int		i, j, count;
	short		*in = (short *) (mod_base + l->fileofs);
	msurface_t **out;

	if (l->filelen % sizeof (short))
		ri.Sys_Error (ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (short);
	out = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, count * sizeof (msurface_t *));

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for (i = 0; i < count; i++)
	{
		j = (unsigned short) LittleShort (in[i]);

		if (j >= loadmodel->numsurfaces)
			ri.Sys_Error (ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");

		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l, dbsp_t *bsp)
{
	int		i, count;
	int		*in = (int *) (mod_base + l->fileofs);

	if (l->filelen % sizeof (int))
		ri.Sys_Error (ERR_DROP, "Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (int);

	for (i = 0; i < count; i++)
		in[i] = LittleLong (in[i]);

	bsp->surfedges = in;
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l, dbsp_t *bsp)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in = (dplane_t *) (mod_base + l->fileofs);
	int			count;

	if (l->filelen % sizeof (dplane_t))
		ri.Sys_Error (ERR_DROP, "Mod_LoadPlanes: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof (dplane_t);

	// this had an extra "* 2" - which was also in GLQuake and may be a legacy from when an earlier varaint didn't have SURF_PLANEBACK
	out = HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, count * sizeof (cplane_t));

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
			out->normal[j] = LittleFloat (in->normal[j]);

		out->type = LittleLong (in->type);
		out->dist = LittleFloat (in->dist);
		out->signbits = Mod_SignbitsForPlane (out);
	}
}


void Mod_SetupSubmodels (model_t *mod)
{
	int i;

	// set up the submodels
	for (i = 0; i < mod->numsubmodels; i++)
	{
		model_t	*starmod = &mod_inline[i];
		mmodel_t *bm = &mod->submodels[i];

		*starmod = *loadmodel;

		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;

		if (starmod->firstnode >= loadmodel->numnodes)
			ri.Sys_Error (ERR_DROP, "Inline model %i has bad firstnode", i);

		Vector3Copy (starmod->maxs, bm->maxs);
		Vector3Copy (starmod->mins, bm->mins);
		starmod->radius = bm->radius;

		if (i == 0)
			*loadmodel = *starmod;

		starmod->numleafs = bm->visleafs;
	}
}


/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i;
	dheader_t	*header;
	//mmodel_t 	*bm;
	dbsp_t		bsp;

	loadmodel->type = mod_brush;

	if (loadmodel != mod_known)
		ri.Sys_Error (ERR_DROP, "Loaded a brush model after the world");

	header = (dheader_t *) buffer;
	i = LittleLong (header->version);

	if (i != BSPVERSION)
		ri.Sys_Error (ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

	// swap all the lumps
	mod_base = (byte *) header;

	for (i = 0; i < sizeof (dheader_t) / 4; i++)
		((int *) header)[i] = LittleLong (((int *) header)[i]);

	// load into heap
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES], &bsp);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES], &bsp);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES], &bsp);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING], &bsp); // if it wasn't for r_lightlevel we could get rid of this too...
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES], &bsp);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO], &bsp);
	Mod_LoadFaces (&header->lumps[LUMP_FACES], &bsp);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES], &bsp);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY], &bsp);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], &bsp);
	Mod_LoadNodes (&header->lumps[LUMP_NODES], &bsp);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS], &bsp);

	// regular and alternate animation
	mod->numframes = 2;

	Mod_SetupSubmodels (mod);
}

