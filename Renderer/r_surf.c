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
// GL_RSURF.C: surface-related refresh code
#include <assert.h>

#include "r_local.h"

#define MAX_SURF_INDEXES	0x100000

static vec3_t	modelorg;		// relative to viewpoint

msurface_t	*r_sky_surfaces;
msurface_t	*r_alpha_surfaces;

static int r_numsurfaceverts = 0;

void R_DrawSkyChain (msurface_t *surf);


int d3d_FirstSurfIndex = 0;

ID3D11Buffer *d3d_SurfVertexes = NULL;
ID3D11Buffer *d3d_SurfIndexes = NULL;

static int d3d_SurfBasicShader;
static int d3d_SurfLightmapShader;
static int d3d_SurfDynamicShader;
static int d3d_SurfDrawTurbShader;

typedef struct brushpolyvert_s {
	float xyz[3];
	float st[2];
	float lm[2];
	float mapnum;
	byte styles[4];
	float scroll;
} brushpolyvert_t;

void R_InitSurfaces (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		VDECL ("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		VDECL ("LIGHTMAP", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0),
		VDECL ("MAPNUM", 0, DXGI_FORMAT_R32_FLOAT, 0, 0),
		VDECL ("STYLES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 0),
		VDECL ("SCROLL", 0, DXGI_FORMAT_R32_FLOAT, 0, 0)
	};

	D3D11_BUFFER_DESC ibDesc = {
		MAX_SURF_INDEXES * sizeof (unsigned int),
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_INDEX_BUFFER,
		D3D11_CPU_ACCESS_WRITE,
		0,
		0
	};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, NULL, &d3d_SurfIndexes);

	d3d_SurfBasicShader = D_CreateShaderBundle (IDR_SURFSHADER, "SurfBasicVS", NULL, "SurfBasicPS", DEFINE_LAYOUT (layout));
	d3d_SurfLightmapShader = D_CreateShaderBundle (IDR_SURFSHADER, "SurfLightmapVS", NULL, "SurfLightmapPS", DEFINE_LAYOUT (layout));
	d3d_SurfDrawTurbShader = D_CreateShaderBundle (IDR_SURFSHADER, "SurfDrawTurbVS", NULL, "SurfDrawTurbPS", DEFINE_LAYOUT (layout));
	d3d_SurfDynamicShader = D_CreateShaderBundle (IDR_SURFSHADER, "SurfDynamicVS", "SurfDynamicGS", "GenericDynamicPS", DEFINE_LAYOUT (layout));
}


void R_ShutdownSurfaces (void)
{
	SAFE_RELEASE (d3d_SurfVertexes);
	SAFE_RELEASE (d3d_SurfIndexes);
}


/*
=============================================================

BRUSH MODELS

=============================================================
*/

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
image_t *R_TextureAnimation (mtexinfo_t *tex, int frame)
{
	if (tex->next)
	{
		int c = frame % tex->numframes;

		while (c)
		{
			tex = tex->next;
			c--;
		}
	}

	return tex->image;
}


/*
================
DrawTextureChains
================
*/
static unsigned int *r_dipindexes = NULL;
static int r_numdipindexes = 0;


void R_WriteSurfaceIndexes (const msurface_t *surf, unsigned int *ndx)
{
	int v;

	// write in the indexes
	for (v = 2; v < surf->numedges; v++, ndx += 3)
	{
		ndx[0] = surf->firstvertex;
		ndx[1] = surf->firstvertex + (v - 1);
		ndx[2] = surf->firstvertex + v;
	}

	// accumulate indexes count
	r_numdipindexes += surf->numindexes;
}


void R_AddSurfaceToBatch (msurface_t *surf)
{
	if (d3d_FirstSurfIndex + r_numdipindexes + surf->numindexes >= MAX_SURF_INDEXES)
	{
		R_EndSurfaceBatch ();
		d3d_FirstSurfIndex = 0;
	}

	if (!r_dipindexes)
	{
		// first index is only reset to 0 if the buffer must wrap so this is valid to do
		D3D11_MAP mode = (d3d_FirstSurfIndex > 0) ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
		D3D11_MAPPED_SUBRESOURCE msr;

		if (FAILED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) d3d_SurfIndexes, 0, mode, 0, &msr)))
			return;
		else r_dipindexes = (unsigned int *) msr.pData + d3d_FirstSurfIndex;
	}

	R_WriteSurfaceIndexes (surf, &r_dipindexes[r_numdipindexes]);
}


void R_EndSurfaceBatch (void)
{
	if (r_dipindexes)
	{
		d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) d3d_SurfIndexes, 0);
		r_dipindexes = NULL;
	}

	if (r_numdipindexes)
	{
		d3d_Context->lpVtbl->DrawIndexed (d3d_Context, r_numdipindexes, d3d_FirstSurfIndex, 0);

		d3d_FirstSurfIndex += r_numdipindexes;
		r_numdipindexes = 0;
	}
}


image_t *R_SelectSurfaceTexture (mtexinfo_t *ti, int frame)
{
	if (r_lightmap->value)
	{
		if (ti->flags & SURF_WARP)
			return r_whitetexture;
		else return r_greytexture;
	}
	else
	{
		if (ti->flags & SURF_WARP)
			return ti->image;
		else return R_TextureAnimation (ti, frame);
	}
}


void R_DrawTextureChains (entity_t *e, model_t *mod, QMATRIX *localmatrix, float alphaval)
{
	int	i;
	msurface_t	*surf;

	// and now set it up
	R_UpdateEntityConstants (localmatrix, NULL, alphaval, 0);

	D_BindVertexBuffer (0, d3d_SurfVertexes, sizeof (brushpolyvert_t), 0);
	D_BindIndexBuffer (d3d_SurfIndexes, DXGI_FORMAT_R32_UINT);

	if (e->flags & RF_TRANSLUCENT)
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSNoCull);
	else D_SetRenderStates (d3d_BSNone, d3d_DSFullDepth, d3d_RSFullCull);

	for (i = 0; i < mod->numtexinfo; i++)
	{
		mtexinfo_t *ti = &mod->texinfo[i];

		// no surfaces
		if ((surf = ti->texturechain) == NULL) continue;

		// select the correct shader
		if (ti->flags & SURF_WARP)
			D_BindShaderBundle (d3d_SurfDrawTurbShader);
		else if (!r_worldmodel->lightdata || r_fullbright->value)
			D_BindShaderBundle (d3d_SurfBasicShader);
		else if (e->flags & RF_TRANSLUCENT)
			D_BindShaderBundle (d3d_SurfBasicShader);
		else D_BindShaderBundle (d3d_SurfLightmapShader);

		// select the correct texture
		R_BindTexture (R_SelectSurfaceTexture (ti, e->currframe)->SRV);

		for (; surf; surf = surf->texturechain)
		{
			R_AddSurfaceToBatch (surf);
			surf->dlightframe = r_dlightframecount;
		}

		R_EndSurfaceBatch ();
		ti->texturechain = NULL;
	}

	if (r_sky_surfaces)
	{
		R_DrawSkyChain (r_sky_surfaces);
		r_sky_surfaces = NULL;
	}

	// no dlights in fullbright mode
	if (!r_worldmodel->lightdata || r_fullbright->value) return;

	// no dlights on translucent entities
	if (e->flags & RF_TRANSLUCENT) return;

	// add dynamic lighting to the entity
	if (mod == r_worldmodel)
		R_PushDlights (mod->nodes, e, mod, localmatrix, r_visframecount);
	else R_PushDlights (mod->nodes + mod->firstnode, e, mod, localmatrix, 0);
}


void R_DrawDlightChains (entity_t *e, model_t *mod, QMATRIX *localmatrix)
{
	int	i;
	msurface_t	*surf;

	for (i = 0; i < mod->numtexinfo; i++)
	{
		mtexinfo_t *ti = &mod->texinfo[i];

		// no surfaces
		if ((surf = ti->texturechain) == NULL) continue;

		D_BindShaderBundle (d3d_SurfDynamicShader);

		// select the correct texture
		R_BindTexture (R_SelectSurfaceTexture (ti, e->currframe)->SRV);

		for (; surf; surf = surf->texturechain)
			R_AddSurfaceToBatch (surf);

		R_EndSurfaceBatch ();
		ti->texturechain = NULL;
	}
}


/*
================
R_DrawAlphaSurfaces

Draw water surfaces and windows.
The BSP tree is waled front to back, so unwinding the chain
of alpha_surfaces will draw back to front, giving proper ordering.
================
*/
void R_DrawAlphaSurfaces (void)
{
	msurface_t	*s;
	QMATRIX	localMatrix;
	image_t *lasttexture = NULL;
	float thisalpha = 1;
	float lastalpha = -1;

	if (!r_alpha_surfaces) return;

	R_MatrixIdentity (&localMatrix);

	D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSNoCull);
	D_BindVertexBuffer (0, d3d_SurfVertexes, sizeof (brushpolyvert_t), 0);
	D_BindIndexBuffer (d3d_SurfIndexes, DXGI_FORMAT_R32_UINT);

	for (s = r_alpha_surfaces; s; s = s->texturechain)
	{
		if (s->texinfo->flags & SURF_TRANS33)
			thisalpha = 0.33f;
		else if (s->texinfo->flags & SURF_TRANS66)
			thisalpha = 0.66f;
		else thisalpha = 1.0f;

		if (thisalpha != lastalpha)
		{
			R_EndSurfaceBatch ();
			R_UpdateEntityConstants (&localMatrix, NULL, thisalpha, 0); // there must be a better way of pushing an alpha value
			lastalpha = thisalpha;
		}

		if (s->texinfo->image != lasttexture)
		{
			R_EndSurfaceBatch ();

			R_BindTexture (s->texinfo->image->SRV);

			if (s->texinfo->flags & SURF_WARP)
				D_BindShaderBundle (d3d_SurfDrawTurbShader);
			else D_BindShaderBundle (d3d_SurfBasicShader);

			lasttexture = s->texinfo->image;
		}

		R_AddSurfaceToBatch (s);
	}

	R_EndSurfaceBatch ();
	r_alpha_surfaces = NULL;
}


void R_ChainSurface (msurface_t *surf)
{
	if (surf->texinfo->flags & SURF_SKY)
	{
		// add to the sky chain
		surf->texturechain = r_sky_surfaces;
		r_sky_surfaces = surf;
	}
	else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
	{
		// add to the translucent chain
		surf->texturechain = r_alpha_surfaces;
		r_alpha_surfaces = surf;
	}
	else
	{
		// normal texture chain
		surf->texturechain = surf->texinfo->texturechain;
		surf->texinfo->texturechain = surf;
	}
}


/*
=================
R_DrawInlineBModel
=================
*/
void R_DrawInlineBModel (entity_t *e, QMATRIX *localmatrix)
{
	int			i;
	int			numsurfaces = 0;
	model_t		*mod = e->model;
	msurface_t	*psurf = &mod->surfaces[mod->firstmodelsurface];

	// draw texture
	for (i = 0; i < mod->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		cplane_t *pplane = psurf->plane;
		float dot = Vector3Dot (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_ChainSurface (psurf);
			numsurfaces++;
		}
	}

	if (numsurfaces)
	{
		if (e->flags & RF_TRANSLUCENT)
			R_DrawTextureChains (e, mod, localmatrix, 0.25f);
		else R_DrawTextureChains (e, mod, localmatrix, 1.0f);
	}
}


/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	vec3_t		mins, maxs;
	int			i;
	QMATRIX		localmatrix;
	model_t		*mod = e->model;

	if (mod->nummodelsurfaces == 0)
		return;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		for (i = 0; i < 3; i++)
		{
			mins[i] = e->currorigin[i] - mod->radius;
			maxs[i] = e->currorigin[i] + mod->radius;
		}
	}
	else
	{
		VectorAdd (e->currorigin, mod->mins, mins);
		VectorAdd (e->currorigin, mod->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	// get the transform in local space so that we can correctly handle dlights
	R_MatrixIdentity (&localmatrix);
	R_MatrixTranslate (&localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotate (&localmatrix, e->angles[0], e->angles[1], e->angles[2]);

	R_VectorInverseTransform (&localmatrix, modelorg, r_newrefdef.vieworg);

	R_DrawInlineBModel (e, &localmatrix);
}


/*
=============================================================

WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node, int clipflags)
{
	int			c, side, sidebit;
	cplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	float		dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

#if 1
	if (clipflags)
	{
		for (c = 0; c < 4; c++)
		{
			// don't need to clip against it
			if (!(clipflags & (1 << c))) continue;

			// the frustum planes are *never* axial so there's no point in doing the "fast" pre-test
			side = BoxOnPlaneSide (node->mins, node->maxs, &frustum[c]);

			if (side == 1) clipflags &= ~(1 << c);	// node is entirely on screen on this side
			if (side == 2) return;	// node is entirely off screen
		}
	}
#else
	if (R_CullBox (node->mins, node->maxs))
		return;
#endif

	// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *) node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (!(r_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
				return;		// not visible
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		return;
	}

	// node is just a decision point, so go down the apropriate sides
	// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = Vector3Dot (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side], clipflags);

	// draw stuff
	for (c = node->numsurfaces, surf = node->surfaces; c; c--, surf++)
	{
		if (surf->visframe != r_framecount)
			continue;

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;		// wrong side

		if (R_CullBoxClipflags (surf->mins, surf->maxs, clipflags))
			continue;

		R_ChainSurface (surf);
	}

	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side], clipflags);
}


/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t ent;
	QMATRIX	localMatrix;

	if (!r_drawworld->value)
		return;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	Vector3Copy (modelorg, r_newrefdef.vieworg);

	// auto cycle the world frame for texture animation
	memset (&ent, 0, sizeof (ent));
	ent.currframe = (int) (r_newrefdef.time * 2);
	ent.model = r_worldmodel;

	R_MatrixIdentity (&localMatrix);

	R_RecursiveWorldNode (r_worldmodel->nodes, 15);
	R_DrawTextureChains (&ent, r_worldmodel, &localMatrix, 1.0f);
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	byte	fatvis[MAX_MAP_LEAFS / 8];
	mnode_t	*node;
	int		i, c;
	mleaf_t	*leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where the pvs ends
	if (gl_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i = 0; i < r_worldmodel->numleafs; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i = 0; i < r_worldmodel->numnodes; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);

	// may have to combine two clusters because of solid water boundaries
	if (r_viewcluster2 != r_viewcluster)
	{
		memcpy (fatvis, vis, (r_worldmodel->numleafs + 7) / 8);
		vis = Mod_ClusterPVS (r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs + 31) / 32;

		for (i = 0; i < c; i++)
			((int *) fatvis)[i] |= ((int *) vis)[i];

		vis = fatvis;
	}

	for (i = 0, leaf = r_worldmodel->leafs; i < r_worldmodel->numleafs; i++, leaf++)
	{
		cluster = leaf->cluster;

		if (cluster == -1)
			continue;

		if (vis[cluster >> 3] & (1 << (cluster & 7)))
		{
			node = (mnode_t *) leaf;

			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}



/*
================
GL_BuildPolygonFromSurface
================
*/
void GL_BuildPolygonFromSurface (msurface_t *surf, model_t *mod, brushpolyvert_t *verts)
{
	int	i, maps;
	byte styles[4] = {0, 0, 0, 0};

	// set up styles
	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		styles[maps] = surf->styles[maps];

	// reconstruct the polygon
	for (i = 0; i < surf->numedges; i++, verts++)
	{
		int lindex = mod->surfedges[surf->firstedge + i];
		float s, t, *vec;

		if (lindex > 0)
		{
			medge_t *r_pedge = &mod->edges[lindex];
			vec = mod->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			medge_t *r_pedge = &mod->edges[-lindex];
			vec = mod->vertexes[r_pedge->v[1]].position;
		}

		VectorCopy (vec, verts->xyz);

		if (surf->texinfo->flags & SURF_WARP)
		{
			// precalc as much of this as possible so that we don't need a more complex FS
			verts->st[0] = Vector3Dot (vec, surf->texinfo->vecs[0]) * 0.015625f;
			verts->st[1] = Vector3Dot (vec, surf->texinfo->vecs[1]) * 0.015625f;

			verts->lm[0] = Vector3Dot (vec, surf->texinfo->vecs[1]) * M_PI / 64.0f;
			verts->lm[1] = Vector3Dot (vec, surf->texinfo->vecs[0]) * M_PI / 64.0f;
		}
		else if (!(surf->texinfo->flags & SURF_SKY))
		{
			s = (Vector3Dot (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) / surf->texinfo->image->width;
			t = (Vector3Dot (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) / surf->texinfo->image->height;

			verts->st[0] = s;
			verts->st[1] = t;

			// lightmap texture coordinates
			s = Vector3Dot (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
			s -= surf->texturemins[0];
			s += surf->light_s * 16;
			s += 8;
			s /= LIGHTMAP_SIZE * 16;

			t = Vector3Dot (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
			t -= surf->texturemins[1];
			t += surf->light_t * 16;
			t += 8;
			t /= LIGHTMAP_SIZE * 16;

			verts->lm[0] = s;
			verts->lm[1] = t;

			// lightmap texture num is texture array slice to use
			verts->mapnum = surf->lightmaptexturenum;

			// copy over styles
			*((unsigned *) verts->styles) = *((unsigned *) styles);
		}

		// copy over scroll factor
		if (surf->texinfo->flags & SURF_FLOWING)
			verts->scroll = 1.0f;
		else verts->scroll = 0.0f;
	}
}


void GL_BeginBuildingSurfaces (model_t *mod)
{
	SAFE_RELEASE (d3d_SurfVertexes);
	r_numsurfaceverts = 0;
}


void R_RegisterSurface (msurface_t *surf)
{
	surf->firstvertex = r_numsurfaceverts;
	surf->numindexes = (surf->numedges - 2) * 3;
	r_numsurfaceverts += surf->numedges;
}


void GL_EndBuildingSurfaces (model_t *mod)
{
	int i;

	D3D11_BUFFER_DESC vbDesc = {
		sizeof (brushpolyvert_t) * r_numsurfaceverts,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	brushpolyvert_t *verts = (brushpolyvert_t *) ri.Load_AllocMemory (sizeof (brushpolyvert_t) * r_numsurfaceverts);

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = {verts, 0, 0};

	// fill in the verts
	for (i = 0; i < mod->numsurfaces; i++)
	{
		msurface_t *surf = &mod->surfaces[i];
		GL_BuildPolygonFromSurface (surf, mod, &verts[surf->firstvertex]);
	}

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &d3d_SurfVertexes);

	// for the next map
	ri.Load_FreeMemory ();
	r_numsurfaceverts = 0;
}

