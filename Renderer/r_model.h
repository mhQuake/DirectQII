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

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//
typedef struct mmodel_s {
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	float		radius;
	int			headnode;
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} mmodel_t;


#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2


#define	SURF_PLANEBACK		2

typedef struct mtexinfo_s {
	float		vecs[2][4];
	int			flags;
	int			numframes;
	struct mtexinfo_s	*next;		// animation chain
	image_t		*image;
} mtexinfo_t;


typedef struct msurface_s {
	int			visframe;		// should be drawn when node is crossed

	cplane_t	*plane;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges

	int			firstvertex;
	int			numindexes;

	float		mins[3];
	float		maxs[3];

	short		texturemins[2];
	short		extents[2];

	int			light_s, light_t;	// gl lightmap coordinates

	struct	msurface_s	*texturechain;
	struct	msurface_s	*reversechain;

	mtexinfo_t	*texinfo;

	// lighting info
	int			dlightframe;

	int			lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];
	byte		*samples;		// [numstyles * surfsize]
} msurface_t;

typedef struct mnode_s {
	// common with leaf
	int			contents;		// -1, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	float		mins[3];		// for bounding box culling
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

	// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];

	msurface_t			*surfaces;
	unsigned short		numsurfaces;
} mnode_t;


typedef struct mleaf_s {
	// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		mins[3];		// for bounding box culling
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

	// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mleaf_t;


//===================================================================

//
// Whole model
//

typedef enum { mod_bad, mod_brush, mod_sprite, mod_alias } modtype_t;


typedef struct dbsp_s {
	// stores BSP file data that is only used during loading and can be thrown away afterwards
	dvertex_t	*vertexes;
	int			*surfedges;
	dedge_t		*edges;
} dbsp_t;


typedef struct maliasframe_s {
	float		scale[3];	// multiply byte verts by this
	float		translate[3];	// then add this
} maliasframe_t;


typedef struct mmdl_s {
	int			skinwidth;
	int			skinheight;

	int			num_skins;
	int			num_tris;
	int			num_verts;
	int			num_indexes;
	int			num_frames;

	char			*skinnames[MAX_MD2SKINS];
	maliasframe_t	*frames;
} mmdl_t;


typedef struct model_s {
	char		name[MAX_QPATH];

	int			registration_sequence;

	modtype_t	type;
	int			numframes;

	int			flags;

	// volume occupied by the model graphics
	vec3_t		mins, maxs;
	float		radius;

	// solid volume for clipping 
	qboolean	clipbox;
	vec3_t		clipmins, clipmaxs;

	// brush model
	int			firstmodelsurface;
	int			nummodelsurfaces;

	int			numsubmodels;
	mmodel_t	*submodels;

	int			numplanes;
	cplane_t	*planes;

	int			numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int			numnodes;
	int			firstnode;
	mnode_t		*nodes;

	int			numtexinfo;
	mtexinfo_t	*texinfo;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	dvis_t		*vis;
	byte		*lightdata;

	// buffer set used
	int			bufferset;

	// for alias models and sprites
	image_t		*skins[MAX_MD2SKINS];

	// Heap memory handle for this model
	HANDLE		hHeap;

	// headers for MD2 and SPR (to do - move BSP here too)
	// these were a union but we want it to be more explicit and make it an error if the wrong header type is accessed for a model
	mmdl_t *md2header;
	dsprite_t *sprheader;
} model_t;


//============================================================================

#define	MAX_MOD_KNOWN	512

void Mod_Init (void);
void Mod_ClearAll (void);
model_t *Mod_ForName (char *name, qboolean crash);
mleaf_t *Mod_PointInLeaf (float *p, model_t *model);
byte *Mod_ClusterPVS (int cluster, model_t *model);
float Mod_RadiusFromBounds (vec3_t mins, vec3_t maxs);
int Mod_SignbitsForPlane (cplane_t *out);

void Mod_FreeAll (void);
void Mod_Free (model_t *mod);

