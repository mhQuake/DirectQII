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
// r_light.c

#include "r_local.h"

// starting the count at 1 so that a memset-0 doesn't mark surfaces
int	r_dlightframecount = 1;

// 256x256 lightmaps allows 4x the surfs packed in a single map
#define	LIGHTMAP_SIZE		256

// using a texture array we must constrain MAX_LIGHTMAPS to this value
#define	MAX_LIGHTMAPS	D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t			pointcolor;
cplane_t		*lightplane;		// used as shadow plane
vec3_t			lightspot;

int R_RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	float		front, back, frac;
	int			side;
	cplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	int			maps;
	int			r;

	if (node->contents != -1)
		return -1;		// didn't hit anything

	// calculate mid point
	// FIXME: optimize for axial
	plane = node->plane;
	front = Vector3Dot (start, plane->normal) - plane->dist;
	back = Vector3Dot (end, plane->normal) - plane->dist;
	side = front < 0;

	if ((back < 0) == side)
		return R_RecursiveLightPoint (node->children[side], start, end);

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side	
	if ((r = R_RecursiveLightPoint (node->children[side], start, mid)) >= 0)
		return r;		// hit something

	if ((back < 0) == side)
		return -1;		// didn't hit anuthing

	// check for impact on this node
	Vector3Copy (lightspot, mid);
	lightplane = plane;

	surf = node->surfaces;

	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->texinfo->flags & SURF_NOLIGHTMAP) continue;	// no lightmaps

		tex = surf->texinfo;

		s = Vector3Dot (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = Vector3Dot (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		VectorCopy (vec3_origin, pointcolor);

		if (lightmap)
		{
			vec3_t scale;

			lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				for (i = 0; i < 3; i++)
					scale[i] = r_newrefdef.lightstyles[surf->styles[maps]];

				pointcolor[0] += lightmap[0] * scale[0] * (1.0 / 255);
				pointcolor[1] += lightmap[1] * scale[1] * (1.0 / 255);
				pointcolor[2] += lightmap[2] * scale[2] * (1.0 / 255);

				lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
			}
		}

		return 1;
	}

	// go down back side
	return R_RecursiveLightPoint (node->children[!side], mid, end);
}

/*
===============
R_LightPoint
===============
*/
void R_LightPoint (vec3_t p, vec3_t color)
{
	int i;
	vec3_t end, hit, lightspot2;

	if (!r_worldmodel->lightdata)
	{
		Vector3Set (color, 1.0f, 1.0f, 1.0f);
		return;
	}

	// don't go outside of the world's bounds
	Vector3Set (end, p[0], p[1], r_worldmodel->mins[2] - 10.0f);

	if ((R_RecursiveLightPoint (r_worldmodel->nodes, p, end)) < 0)
	{
		// objects outside the world (such as Strogg Viper flybys on base1) should be lit if they hit nothing
		// the new end point (based on mins of r_worldmodel) ensures that we'll always hit if inside the world so this is OK
		// don't make them too bright or they'll look like shit/
		Vector3Set (color, 0.5f, 0.5f, 0.5f);
		Vector3Copy (hit, end);
	}
	else
	{
		Vector3Copy (color, pointcolor);
		Vector3Copy (hit, lightspot);
	}

	// find bmodels under the lightpoint - move the point to bmodel space, trace down, then check; if r < 0
	// it didn't find a bmodel, otherwise it did (a bmodel under a valid world hit will hit here too)
	// fixme: is it possible for a bmodel to not be in the PVS but yet be a valid candidate for this???
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		entity_t *e = &r_newrefdef.entities[i];
		float estart[3], eend[3];

		// beam and translucent ents don't light
		if (e->flags & RF_BEAM) continue;
		if (e->flags & RF_TRANSLUCENT) continue;

		// NULL models don't light
		if (!e->model) continue;

		// only bmodel entities give light
		if (e->model->type != mod_brush) continue;

		// this happens on boss1
		if (e->model->firstnode < 0) continue;

		// move start and end points into the entity's frame of reference
		R_VectorInverseTransform (&r_local_matrix[i], estart, p);
		R_VectorInverseTransform (&r_local_matrix[i], eend, end);

		// and run the recursive light point on it too
		if (!(R_RecursiveLightPoint (e->model->nodes + e->model->firstnode, estart, eend) < 0))
		{
			// a bmodel under a valid world hit will hit here too so take the highest lightspot on all hits
			// move lightspot back to world space
			R_VectorTransform (&r_local_matrix[i], lightspot2, lightspot);

			if (lightspot2[2] > hit[2])
			{
				// found a bmodel so copy it over
				Vector3Copy (color, pointcolor);
				Vector3Copy (hit, lightspot2);
			}
		}
	}

	// the final hit point is the valid lightspot
	Vector3Copy (lightspot, hit);
}


void R_DynamicLightPoint (vec3_t p, vec3_t color)
{
	int			lnum;
	dlight_t	*dl = r_newrefdef.dlights;

	// the main passes for alias models don't add dynamics because they use realtime lighting instead;
	// passes for null models and setting r_lightlevel do
	for (lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		float add = (dl->intensity - Vector3Dist (p, dl->origin)) * (1.0f / 256.0f);

		if (add > 0)
		{
			// add in the light
			Vector3Madf (color, dl->color, add, color);
		}
	}
}


void R_SetEntityLighting (entity_t *e, float *shadelight, float *shadevector)
{
	// split out so that we can use it for NULL models and any others
	float an;

	// PGM	ir goggles override
	if ((r_newrefdef.rdflags & RDF_IRGOGGLES) && (e->flags & RF_IR_VISIBLE))
	{
		shadelight[0] = 1.0;
		shadelight[1] = 0.0;
		shadelight[2] = 0.0;
	}

	an = (e->angles[0] + e->angles[1]) / 180 * M_PI;
	shadevector[0] = cos (-an);
	shadevector[1] = sin (-an);
	shadevector[2] = 1;
	Vector3Normalize (shadevector);
}


/*
=============================================================================

LIGHTMAP ALLOCATION

=============================================================================
*/

typedef struct lighttexel_s {
	byte styles[4];
} lighttexel_t;

static int r_currentlightmap = 0;
static int lm_allocated[LIGHTMAP_SIZE];

static texture_t d3d_Lightmaps[3];

static lighttexel_t **lm_data[3];

static ID3D11Buffer *d3d_DLightConstants = NULL;

static tbuffer_t d3d_QuakePalette; // this is a stupid place to do this
static tbuffer_t d3d_LightNormals; // lightnormals are read on the GPU so that we can use a skinnier vertex format and save loads of memory
static tbuffer_t d3d_LightStyles;

#define NUMVERTEXNORMALS	162

// padded for HLSL usage
// to do - reconstruct this mathematically
static float r_avertexnormals[NUMVERTEXNORMALS][4] = {
#include "../DirectQII/anorms.h"
};

void R_InitLight (void)
{
	D3D11_BUFFER_DESC cbDLightDesc = {
		sizeof (dlight_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbDLightDesc, NULL, &d3d_DLightConstants);
	D_RegisterConstantBuffer (d3d_DLightConstants, 4);

	R_CreateTBuffer (&d3d_LightStyles, NULL, MAX_LIGHTSTYLES, sizeof (float), DXGI_FORMAT_R32_FLOAT);
	R_CreateTBuffer (&d3d_LightNormals, r_avertexnormals, NUMVERTEXNORMALS, sizeof (r_avertexnormals[0]), DXGI_FORMAT_R32G32B32A32_FLOAT);
	R_CreateTBuffer (&d3d_QuakePalette, d_8to24table, 256, sizeof (d_8to24table[0]), DXGI_FORMAT_R8G8B8A8_UNORM); // this is a stupid place to do this
}


void R_ShutdownLightmaps (void)
{
	R_ReleaseTexture (&d3d_Lightmaps[0]);
	R_ReleaseTexture (&d3d_Lightmaps[1]);
	R_ReleaseTexture (&d3d_Lightmaps[2]);
}


void R_ShutdownLight (void)
{
	R_ReleaseTBuffer (&d3d_LightStyles);
	R_ReleaseTBuffer (&d3d_LightNormals);
	R_ReleaseTBuffer (&d3d_QuakePalette); // this is a stupid place to do this

	R_ShutdownLightmaps ();
}


void R_BuildLightMap (msurface_t *surf, int ch, int smax, int tmax)
{
	// create the channel if needed first time it's seen
	if (!lm_data[ch])
	{
		lm_data[ch] = (lighttexel_t **) ri.Load_AllocMemory (MAX_LIGHTMAPS * sizeof (lm_data[ch]));
		memset (lm_data[ch], 0, MAX_LIGHTMAPS * sizeof (lm_data[ch]));
	}

	// create the texture if needed first time it's seen
	if (!lm_data[ch][surf->lightmaptexturenum])
	{
		lm_data[ch][surf->lightmaptexturenum] = (lighttexel_t *) ri.Load_AllocMemory (LIGHTMAP_SIZE * LIGHTMAP_SIZE * sizeof (lighttexel_t));
		memset (lm_data[ch][surf->lightmaptexturenum], 0, LIGHTMAP_SIZE * LIGHTMAP_SIZE * sizeof (lighttexel_t));
	}

	if (surf->samples)
	{
		// copy over the lightmap beginning at the appropriate colour channel
		byte *lightmap = surf->samples;
		int maps;

		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			lighttexel_t *dest = lm_data[ch][surf->lightmaptexturenum] + (surf->light_t * LIGHTMAP_SIZE) + surf->light_s;
			int s, t;

			for (t = 0; t < tmax; t++)
			{
				for (s = 0; s < smax; s++)
				{
					dest[s].styles[maps] = lightmap[ch];	// 'ch' is intentional here
					lightmap += 3;
				}

				dest += LIGHTMAP_SIZE;
			}
		}
	}
}


/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int i;
	int best = LIGHTMAP_SIZE;
	int smax = (surf->extents[0] >> 4) + 1;
	int tmax = (surf->extents[1] >> 4) + 1;

	for (i = 0; i < LIGHTMAP_SIZE - smax; i++)
	{
		int j, best2 = 0;

		for (j = 0; j < smax; j++)
		{
			if (lm_allocated[i + j] >= best) break;
			if (lm_allocated[i + j] > best2) best2 = lm_allocated[i + j];
		}

		if (j == smax)
		{
			// this is a valid spot
			surf->light_s = i;
			surf->light_t = best = best2;
		}
	}

	if (best + tmax > LIGHTMAP_SIZE)
	{
		// go to next lightmap
		if (++r_currentlightmap >= MAX_LIGHTMAPS)
			ri.Sys_Error (ERR_DROP, "GL_CreateSurfaceLightmap : MAX_LIGHTMAPS exceeded");

		// clear allocations
		memset (lm_allocated, 0, sizeof (lm_allocated));

		// and call recursively to fill it in
		GL_CreateSurfaceLightmap (surf);
		return;
	}

	// mark off allocated regions
	for (i = 0; i < smax; i++)
		lm_allocated[surf->light_s + i] = best + tmax;

	// assign the lightmap to the surf
	surf->lightmaptexturenum = r_currentlightmap;

	// and build it's lightmaps
	// each lightmap texture is one of r, g or b and contains 4 styles for it's colour channel
	R_BuildLightMap (surf, 0, smax, tmax);
	R_BuildLightMap (surf, 1, smax, tmax);
	R_BuildLightMap (surf, 2, smax, tmax);
}


/*
==================
GL_BeginBuildingLightmaps

==================
*/
void GL_BeginBuildingLightmaps (model_t *m)
{
	// release prior textures
	R_ShutdownLightmaps ();

	// wipe lightmaps and allocations
	memset (lm_data, 0, sizeof (lm_data));
	memset (lm_allocated, 0, sizeof (lm_allocated));

	// begin with no lightmaps
	r_currentlightmap = 0;

	// fixme - this is an awful place to have this; how about EndRegistration or something like that instead???
	r_framecount = 1;
}


void R_CreateLightmapTexture (int ch)
{
	int i;
	D3D11_SUBRESOURCE_DATA *srd = ri.Load_AllocMemory (sizeof (D3D11_SUBRESOURCE_DATA) * r_currentlightmap);

	// set up the SRDs
	for (i = 0; i < r_currentlightmap; i++)
	{
		srd[i].pSysMem = lm_data[ch][i];
		srd[i].SysMemPitch = LIGHTMAP_SIZE << 2;
		srd[i].SysMemSlicePitch = 0;
	}

	// and create it
	R_CreateTexture (&d3d_Lightmaps[ch], srd, LIGHTMAP_SIZE, LIGHTMAP_SIZE, r_currentlightmap, TEX_RGBA8);
}


/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps (void)
{
	// complete the count of lightmaps
	r_currentlightmap++;

	// create the three textures
	R_CreateLightmapTexture (0);
	R_CreateLightmapTexture (1);
	R_CreateLightmapTexture (2);

	// any further attempt to access these is an error
	memset (lm_data, 0, sizeof (lm_data));
	memset (lm_allocated, 0, sizeof (lm_allocated));

	// hand back memory
	ri.Load_FreeMemory ();
}


void R_SetupLightmapTexCoords (msurface_t *surf, float *vec, float *lm)
{
	lm[0] = Vector3Dot (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
	lm[0] -= surf->texturemins[0];
	lm[0] += surf->light_s * 16;
	lm[0] += 8;
	lm[0] /= LIGHTMAP_SIZE * 16;

	lm[1] = Vector3Dot (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
	lm[1] -= surf->texturemins[1];
	lm[1] += surf->light_t * 16;
	lm[1] += 8;
	lm[1] /= LIGHTMAP_SIZE * 16;
}


void R_BindLightmaps (void)
{
	// VS tbuffers go to VS slots 0/1/2
	ID3D11ShaderResourceView *VertexSRVs[3] = {d3d_LightStyles.SRV, d3d_LightNormals.SRV, d3d_QuakePalette.SRV};

	// lightmap textures go to PS slots 1/2/3
	ID3D11ShaderResourceView *LightmapSRVs[3] = {d3d_Lightmaps[0].SRV, d3d_Lightmaps[1].SRV, d3d_Lightmaps[2].SRV};

	// optionally update lightstyles (this can be NULL)
	if (r_newrefdef.lightstyles)
		d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_LightStyles.Buffer, 0, NULL, r_newrefdef.lightstyles, 0, 0);

	// and set them all
	d3d_Context->lpVtbl->VSSetShaderResources (d3d_Context, 0, 3, VertexSRVs);
	d3d_Context->lpVtbl->PSSetShaderResources (d3d_Context, 1, 3, LightmapSRVs);
}


void D_SetupDynamicLight (dlight_t *dl)
{
	// select the appropriate state (fixme - this rasterizer state is wrong for left-handed gun models)
	if (dl->color[0] < 0 || dl->color[1] < 0 || dl->color[2] < 0)
	{
		// anti-light - flip back to positive and set the appropriate state
		Vector3Scalef (dl->color, dl->color, -1);
		D_SetRenderStates (d3d_BSSubtractive, d3d_DSEqualDepthNoWrite, d3d_RSFullCull);
		d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_DLightConstants, 0, NULL, dl, 0, 0);
		Vector3Scalef (dl->color, dl->color, -1); // put it back so it will be correctly detected next time
	}
	else
	{
		// standard light can just go straight up as-is
		D_SetRenderStates (d3d_BSAdditive, d3d_DSEqualDepthNoWrite, d3d_RSFullCull);
		d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_DLightConstants, 0, NULL, dl, 0, 0);
	}
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
qboolean R_SurfaceDLImpact (msurface_t *surf, dlight_t *dl, float dist)
{
	int s, t;
	float impact[3], l;

	impact[0] = dl->origin[0] - surf->plane->normal[0] * dist;
	impact[1] = dl->origin[1] - surf->plane->normal[1] * dist;
	impact[2] = dl->origin[2] - surf->plane->normal[2] * dist;

	// clamp center of light to corner and check brightness
	l = Vector3Dot (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];
	s = l + 0.5; if (s < 0) s = 0; else if (s > surf->extents[0]) s = surf->extents[0];
	s = l - s;

	l = Vector3Dot (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
	t = l + 0.5; if (t < 0) t = 0; else if (t > surf->extents[1]) t = surf->extents[1];
	t = l - t;

	// compare to minimum light
	return ((s * s + t * t + dist * dist) < (dl->intensity * dl->intensity));
}


void R_MarkLights (dlight_t *dl, mnode_t *node, int visframe)
{
	cplane_t	*splitplane;
	float		dist;
	int			i;

	if (node->contents != -1) return;

	// don't bother tracing nodes that will not be visible; this is *far* more effective than any
	// tricksy recursion "optimization" (which a decent compiler will automatically do for you anyway)
	if (node->visframe != visframe) return;

	splitplane = node->plane;
	dist = Vector3Dot (dl->origin, splitplane->normal) - splitplane->dist;

	if (dist > dl->intensity)
	{
		R_MarkLights (dl, node->children[0], visframe);
		return;
	}

	if (dist < -dl->intensity)
	{
		R_MarkLights (dl, node->children[1], visframe);
		return;
	}

	// mark the polygons
	for (i = 0; i < node->numsurfaces; i++)
	{
		msurface_t *surf = &node->surfaces[i];

		// no lightmaps on these surface types
		if (surf->texinfo->flags & SURF_NOLIGHTMAP) continue;

		// omit surfaces not marked in the current render
		if (surf->dlightframe != r_dlightframecount) continue;

		// test for impact
		if (R_SurfaceDLImpact (surf, dl, dist))
		{
			// chain it for lighting
			surf->texturechain = surf->texinfo->texturechain;
			surf->texinfo->texturechain = surf;

			// record surfaces in this light
			dl->numsurfaces++;
		}
	}

	R_MarkLights (dl, node->children[0], visframe);
	R_MarkLights (dl, node->children[1], visframe);
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (mnode_t *headnode, entity_t *e, model_t *mod, QMATRIX *localmatrix, int visframe)
{
	int	i;

	for (i = 0; i < r_newrefdef.num_dlights; i++)
	{
		float origin[3];
		dlight_t *dl = &r_newrefdef.dlights[i];

		// a dl that's been culled will have it's intensity set to 0
		if (!(dl->intensity > 0)) continue;

		// copy off the origin, then move the light into entity local space
		Vector3Copy (origin, dl->origin);
		R_VectorInverseTransform (localmatrix, dl->origin, origin);

		// no surfaces yet
		dl->numsurfaces = 0;

		// and mark it
		R_MarkLights (dl, headnode, visframe);

		// draw anything we got
		if (dl->numsurfaces)
		{
			D_SetupDynamicLight (dl);
			R_DrawDlightChains (e, mod, localmatrix);
			dl->numsurfaces = 0;
		}

		// restore the origin
		Vector3Copy (dl->origin, origin);
	}

	// go to a new dlight frame after each push so that we don't carry over lights from the previous
	r_dlightframecount++;
}


/*
====================
R_SetLightLevel

====================
*/
void R_SetLightLevel (void)
{
	vec3_t	shadelight;
	float	best = 0.0f;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)
	// this never worked right because R_LightPoint used currententity->origin instead of p for stuff
	// but currententity would not have been set right when this was called - it does work right now.
	// (that's probably why it was also added to R_DrawAliasModel)
	R_LightPoint (r_newrefdef.vieworg, shadelight);
	R_DynamicLightPoint (r_newrefdef.vieworg, shadelight);

	// pick the greatest component, which should be the same as the mono value returned by software
	if (shadelight[0] > best) best = shadelight[0];
	if (shadelight[1] > best) best = shadelight[1];
	if (shadelight[2] > best) best = shadelight[2];

	// scale it up and save it out
	r_lightlevel->value = 150.0f * best;
}


void R_PrepareDlights (void)
{
	// bbcull and other setup for dlights
	int	i;

	for (i = 0; i < r_newrefdef.num_dlights; i++)
	{
		// NOTE ON THE SCISSOR TEST OPTIMIZATION - this was a perf win back in 2004/2005; nowadays it's actually slower
		dlight_t *dl = &r_newrefdef.dlights[i];

		// fast cullsphere elimination first
		if (R_CullSphere (dl->origin, dl->intensity))
		{
			dl->intensity = 0;
			continue;
		}

		// other stuff may follow...
	}
}

