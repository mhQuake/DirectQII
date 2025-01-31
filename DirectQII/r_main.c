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
// r_main.c
#include "r_local.h"

void R_DrawParticles (void);
void R_SetupSky (QMATRIX *SkyMatrix);
void R_SetLightLevel (void);
void R_PrepareDlights (void);


__declspec(align(16)) typedef struct mainconstants_s {
	QMATRIX mvpMatrix;
	float viewOrigin[4]; // padded for cbuffer packing
	float viewForward[4]; // padded for cbuffer packing
	float viewRight[4]; // padded for cbuffer packing
	float viewUp[4]; // padded for cbuffer packing
	float vBlend[4];
	float WarpTime[2];
	float TexScroll;
	float turbTime;
	float RefdefX;
	float RefdefY;
	float RefdefW;
	float RefdefH;
	QMATRIX skyMatrix;
	float frustum0[4];
	float frustum1[4];
	float frustum2[4];
	float frustum3[4];
	float desaturation;
	float beamdetail;
	float junk[2];
} mainconstants_t;

__declspec(align(16)) typedef struct entityconstants_s {
	QMATRIX localMatrix;
	float shadecolor[3];
	float alphaval;
} entityconstants_t;


ID3D11Buffer *d3d_MainConstants = NULL;
ID3D11Buffer *d3d_EntityConstants = NULL;

int d3d_PolyblendShader = 0;

void R_InitMain (void)
{
	D3D11_BUFFER_DESC cbMainDesc = {
		sizeof (mainconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	D3D11_BUFFER_DESC cbEntityDesc = {
		sizeof (entityconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbMainDesc, NULL, &d3d_MainConstants);
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbEntityDesc, NULL, &d3d_EntityConstants);

	D_RegisterConstantBuffer (d3d_MainConstants, 1);
	D_RegisterConstantBuffer (d3d_EntityConstants, 2);

	d3d_PolyblendShader = D_CreateShaderBundle (IDR_DRAWSHADER, "DrawPolyblendVS", NULL, "DrawPolyblendPS", NULL, 0);
}


viddef_t	vid;

refimport_t	ri;

model_t		*r_worldmodel;

glconfig_t gl_config;
glstate_t  gl_state;

image_t		*r_notexture;		// use for bad textures
image_t		*r_blacktexture;	// use for bad textures
image_t		*r_greytexture;		// use for bad textures
image_t		*r_whitetexture;	// use for bad textures

cplane_t	frustum[4];

mleaf_t	*r_viewleaf, *r_oldviewleaf;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;

float		v_blend[4];			// final blending color

// world transforms
QMATRIX	r_view_matrix;
QMATRIX	r_proj_matrix;
QMATRIX	r_mvp_matrix;
QMATRIX r_local_matrix[MAX_ENTITIES];

// view origin
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;

// screen size info
refdef_t	r_newrefdef;

cvar_t	*r_testnullmodels;
cvar_t	*scr_conscale;
cvar_t	*r_lightmap;
cvar_t	*r_testnotexture;
cvar_t	*r_lightmodel;
cvar_t	*r_fullbright;
cvar_t	*r_beamdetail;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_novis;
cvar_t	*r_lefthand;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level
cvar_t	* r_lightsaturation;

cvar_t	*vid_mode;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_polyblend;
cvar_t	*gl_lockpvs;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*vid_brightness;

cvar_t	*vid_width;
cvar_t	*vid_height;
cvar_t	*vid_vsync;


void R_PrepareEntityForRendering (QMATRIX *localMatrix, float *color, float alpha, int rflags)
{
	entityconstants_t consts;

	// we need to retain the local matrix for lighting so transform it into the cBuffer directly
	if (((rflags & RF_WEAPONMODEL) && (r_lefthand->value || r_newrefdef.fovvar > 90.0f)) || (rflags & RF_DEPTHHACK))
	{
		// this will be the matrix that's used as a base for this model
		QMATRIX flipmatrix;
		R_MatrixIdentity (&flipmatrix);

		// scale the matrix to flip from right-handed to left-handed
		if ((rflags & RF_WEAPONMODEL) && r_lefthand->value) R_MatrixScale (&flipmatrix, -1, 1, 1);

		// scale the matrix to provide the depthrange hack (so that we don't need to set a new VP with the modified range)
		if (rflags & RF_DEPTHHACK) R_MatrixScale (&flipmatrix, 1.0f, 1.0f, 0.3f);

		// multiply by MVP for the final matrix, using a separate FOV for the gun if > 90
		if ((rflags & RF_WEAPONMODEL) && r_newrefdef.fovvar > 90.0f)
		{
			float fovscale = tan (r_newrefdef.fovvar * (0.5f * M_PI / 180.0f));
			QMATRIX r_viewmodel_scale = {fovscale, 0, 0, 0, 0, fovscale, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

			R_MatrixMultiply (&flipmatrix, &flipmatrix, &r_proj_matrix);
			R_MatrixMultiply (&flipmatrix, &flipmatrix, &r_viewmodel_scale);
			R_MatrixMultiply (&flipmatrix, &flipmatrix, &r_view_matrix);
		}
		else R_MatrixMultiply (&flipmatrix, &flipmatrix, &r_mvp_matrix);

		R_MatrixMultiply (&consts.localMatrix, &flipmatrix, localMatrix);
	}
	else R_MatrixMultiply (&consts.localMatrix, &r_mvp_matrix, localMatrix);

	// the color param can be NULL indicating white
	if (color)
		Vector3Copy (consts.shadecolor, color);
	else Vector3Set (consts.shadecolor, 1, 1, 1);

	// alpha is only set if we're explicitly translucent
	if (rflags & RF_TRANSLUCENT)
		consts.alphaval = alpha;
	else consts.alphaval = 1.0f;

	// and update to the cbuffer
	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_EntityConstants, 0, NULL, &consts, 0, 0);

	// and set the correct states
	if (rflags & RF_TRANSLUCENT)
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, R_SelectRasterizerState (rflags));
	else D_SetRenderStates (d3d_BSNone, d3d_DSFullDepth, R_SelectRasterizerState (rflags));
}


/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullSphere (float *center, float radius)
{
	if (Mod_PlaneDist (&frustum[0], center) <= -radius) return true;
	if (Mod_PlaneDist (&frustum[1], center) <= -radius) return true;
	if (Mod_PlaneDist (&frustum[2], center) <= -radius) return true;
	if (Mod_PlaneDist (&frustum[3], center) <= -radius) return true;

	return false;
}


qboolean R_CullBox (vec3_t emins, vec3_t emaxs)
{
	int		i;

	for (i = 0; i < 4; i++)
	{
		cplane_t *p = &frustum[i];

		switch (p->signbits)
		{
		default:
		case 0: if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist) return true; break;
		case 1: if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2] < p->dist) return true; break;
		case 2: if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist) return true; break;
		case 3: if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2] < p->dist) return true; break;
		case 4: if (p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist) return true; break;
		case 5: if (p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2] < p->dist) return true; break;
		case 6: if (p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist) return true; break;
		case 7: if (p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2] < p->dist) return true; break;
		}
	}

	return false;
}


qboolean R_CullBoxClipflags (vec3_t mins, vec3_t maxs, int clipflags)
{
	// only used by the surf code now
	int i;
	int side;

	for (i = 0; i < 4; i++)
	{
		// don't need to clip against it
		if (!(clipflags & (1 << i))) continue;

		// the frustum planes are *never* axial so there's no point in doing the "fast" pre-test
		side = BoxOnPlaneSide (mins, maxs, &frustum[i]);

		if (side == 1) clipflags &= ~(1 << i);
		if (side == 2) return true;
	}

	// onscreen
	return false;
}


qboolean R_CullForEntity (vec3_t mins, vec3_t maxs, QMATRIX *localmatrix)
{
	int i;
	vec3_t emins = {999999, 999999, 999999};
	vec3_t emaxs = {-999999, -999999, -999999};

	// compute a full bounding box
	for (i = 0; i < 8; i++)
	{
		vec3_t tmp1, tmp2;

		// get this corner point
		if (i & 1) tmp1[0] = mins[0]; else tmp1[0] = maxs[0];
		if (i & 2) tmp1[1] = mins[1]; else tmp1[1] = maxs[1];
		if (i & 4) tmp1[2] = mins[2]; else tmp1[2] = maxs[2];

		// transform it
		R_VectorTransform (localmatrix, tmp2, tmp1);

		// derive new mins
		if (tmp2[0] < emins[0]) emins[0] = tmp2[0];
		if (tmp2[1] < emins[1]) emins[1] = tmp2[1];
		if (tmp2[2] < emins[2]) emins[2] = tmp2[2];

		// derive new maxs
		if (tmp2[0] > emaxs[0]) emaxs[0] = tmp2[0];
		if (tmp2[1] > emaxs[1]) emaxs[1] = tmp2[1];
		if (tmp2[2] > emaxs[2]) emaxs[2] = tmp2[2];
	}

	// now cullbox using the new derived mins and maxs
	return R_CullBox (emins, emaxs);
}


//==================================================================================


void R_DrawEntitiesOnList (qboolean trans)
{
	int		i;

	if (!r_drawentities->value)
		return;

	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		entity_t *e = &r_newrefdef.entities[i];

		// draw translucent and opaque entities in their proper passes
		if (!trans && (e->flags & RF_TRANSLUCENT)) continue;
		if (trans && !(e->flags & RF_TRANSLUCENT)) continue;

		// optionally skip weapon models
		if ((e->flags & RF_WEAPONMODEL) && (r_newrefdef.rdflags & RDF_NOWEAPONMODEL)) continue;

		if (e->flags & RF_BEAM)
		{
			R_DrawBeam (e, &r_local_matrix[i]);
		}
		else
		{
			if (!e->model)
			{
				R_DrawNullModel (e, &r_local_matrix[i]);
				continue;
			}

			switch (e->model->type)
			{
			case mod_alias:
				R_DrawMeshEntity (e, &r_local_matrix[i]);
				break;

			case mod_brush:
				R_DrawBrushModel (e, &r_local_matrix[i]);
				break;

			case mod_sprite:
				R_DrawSpriteModel (e, &r_local_matrix[i]);
				break;

			case mod_md5:
				R_DrawMeshEntity (e, &r_local_matrix[i]);
				break;

			default:
				ri.Sys_Error (ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}
}


//=======================================================================


/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	r_framecount++;

	// current viewcluster
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL) || (r_worldmodel != NULL))
		r_viewleaf = Mod_PointInLeaf (r_newrefdef.vieworg, r_worldmodel);

	// scale for value of gl_polyblend
	Vector4Copy (v_blend, r_newrefdef.blend);
	v_blend[3] *= gl_polyblend->value;

	c_brush_polys = 0;
	c_alias_polys = 0;
}


void R_ExtractFrustum (cplane_t *f, QMATRIX *m)
{
	int i;

	// extract the frustum from the MVP matrix
	f[0].normal[0] = m->m4x4[0][3] - m->m4x4[0][0];
	f[0].normal[1] = m->m4x4[1][3] - m->m4x4[1][0];
	f[0].normal[2] = m->m4x4[2][3] - m->m4x4[2][0];

	f[1].normal[0] = m->m4x4[0][3] + m->m4x4[0][0];
	f[1].normal[1] = m->m4x4[1][3] + m->m4x4[1][0];
	f[1].normal[2] = m->m4x4[2][3] + m->m4x4[2][0];

	f[2].normal[0] = m->m4x4[0][3] + m->m4x4[0][1];
	f[2].normal[1] = m->m4x4[1][3] + m->m4x4[1][1];
	f[2].normal[2] = m->m4x4[2][3] + m->m4x4[2][1];

	f[3].normal[0] = m->m4x4[0][3] - m->m4x4[0][1];
	f[3].normal[1] = m->m4x4[1][3] - m->m4x4[1][1];
	f[3].normal[2] = m->m4x4[2][3] - m->m4x4[2][1];

	for (i = 0; i < 4; i++)
	{
		Vector3Normalize (f[i].normal);
		frustum[i].type = PLANE_ANYZ;
		f[i].dist = Vector3Dot (r_newrefdef.vieworg, f[i].normal);
		f[i].signbits = Mod_SignbitsForPlane (&f[i]);
	}
}


float R_GetFarClip (void)
{
	// never go below the standard farclip of 4096
	float farclip = 4096;

	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		// this provides the maximum far clip per view position and worldmodel bounds
		for (int i = 0; i < 8; i++)
		{
			float dist;
			vec3_t corner;

			// get this corner point
			if (i & 1) corner[0] = r_worldmodel->mins[0]; else corner[0] = r_worldmodel->maxs[0];
			if (i & 2) corner[1] = r_worldmodel->mins[1]; else corner[1] = r_worldmodel->maxs[1];
			if (i & 4) corner[2] = r_worldmodel->mins[2]; else corner[2] = r_worldmodel->maxs[2];

			if ((dist = Vector3Dist (r_newrefdef.vieworg, corner)) > farclip)
				farclip = dist;
		}
	}

	return farclip;
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	mainconstants_t consts;
	double junk[2] = {0, 0};

	// near and far clipping plane distances; the far clipping plane dist is based on worldmodel bounds and view position
	float r_nearclip = 4.0f;
	float r_farclip = R_GetFarClip ();

	// set up the viewport that we'll use for the entire refresh
	D3D11_VIEWPORT vp = {r_newrefdef.x, r_newrefdef.y, r_newrefdef.width, r_newrefdef.height, 0, 1};
	d3d_Context->lpVtbl->RSSetViewports (d3d_Context, 1, &vp);

	// the projection matrix may be only updated when the refdef changes but we do it every frame so that we can do waterwarp
	R_MatrixIdentity (&r_proj_matrix);
	R_MatrixFrustum (&r_proj_matrix, r_newrefdef.fov.x, r_newrefdef.fov.y, r_nearclip, r_farclip);

	// modelview is updated every frame; done in reverse so that we can use the new sexy rotation on it
	R_MatrixIdentity (&r_view_matrix);
	R_MatrixCamera (&r_view_matrix, r_newrefdef.vieworg, r_newrefdef.viewangles);

	// compute the global MVP
	R_MatrixMultiply (&r_mvp_matrix, &r_proj_matrix, &r_view_matrix);

	// and extract the frustum from it
	R_ExtractFrustum (frustum, &r_mvp_matrix);

	// take these from the view matrix
	Vector3Set (vpn,   -r_view_matrix.m4x4[0][2], -r_view_matrix.m4x4[1][2], -r_view_matrix.m4x4[2][2]);
	Vector3Set (vup,    r_view_matrix.m4x4[0][1],  r_view_matrix.m4x4[1][1],  r_view_matrix.m4x4[2][1]);
	Vector3Set (vright, r_view_matrix.m4x4[0][0],  r_view_matrix.m4x4[1][0],  r_view_matrix.m4x4[2][0]);

	// setup the shader constants that are going to remain unchanged for the frame; time-based animations, etc
	R_MatrixLoad (&consts.mvpMatrix, &r_mvp_matrix);

	Vector3Copy (consts.viewOrigin, r_newrefdef.vieworg);
	Vector3Copy (consts.viewForward, vpn);
	Vector3Copy (consts.viewRight, vright);
	Vector3Copy (consts.viewUp, vup);

	Vector4Copy (consts.vBlend, v_blend);

	consts.WarpTime[0] = modf (r_newrefdef.time * 0.09f, &junk[0]);
	consts.WarpTime[1] = modf (r_newrefdef.time * 0.11f, &junk[1]);

	consts.TexScroll = modf ((double) r_newrefdef.time / 40.0, &junk[0]) * -64.0f;

	// cycle in increments of 2 * M_PI so that the sine warp will wrap correctly
	consts.turbTime = r_newrefdef.time - ((M_PI * 2) * (int) (r_newrefdef.time / (M_PI * 2)));

	// copy over refdef size for stuff like e.g. the water warp noise lookup
	consts.RefdefX = r_newrefdef.x;
	consts.RefdefY = r_newrefdef.y;
	consts.RefdefW = r_newrefdef.width;
	consts.RefdefH = r_newrefdef.height;

	// set up sky for drawing (also binds the sky texture)
	R_SetupSky (&consts.skyMatrix);

	// copy off the frustum
	Vector4Copy (consts.frustum0, frustum[0].normal);	// overflow is deliberate
	Vector4Copy (consts.frustum1, frustum[1].normal);	// overflow is deliberate
	Vector4Copy (consts.frustum2, frustum[2].normal);	// overflow is deliberate
	Vector4Copy (consts.frustum3, frustum[3].normal);	// overflow is deliberate

	if (r_lightsaturation->value < 0)
		consts.desaturation = 0;
	else if (r_lightsaturation->value > 1)
		consts.desaturation = 1;
	else consts.desaturation = r_lightsaturation->value;

	// this should match the detail setup in R_CreateBeamVertexes
	if (r_beamdetail->value < 3)
		consts.beamdetail = 3;
	else if (r_beamdetail->value > 360)
		consts.beamdetail = 360;
	else consts.beamdetail = r_beamdetail->value;

	// and update to the cbuffer
	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_MainConstants, 0, NULL, &consts, 0, 0);

	// clear out the portion of the screen that the NOWORLDMODEL defines
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		// we can't clear subrects in D3D11 so just clear the entire thing
		d3d_Context->lpVtbl->ClearDepthStencilView (d3d_Context, d3d_DepthBuffer, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 1);
	}
}


/*
=============
R_Clear
=============
*/
void R_Clear (ID3D11RenderTargetView *RTV, ID3D11DepthStencilView *DSV)
{
	mleaf_t *leaf = Mod_PointInLeaf (r_newrefdef.vieworg, r_worldmodel);

	if (leaf->contents & CONTENTS_SOLID)
	{
		// if the view is inside solid it's probably because we're noclipping so we need to clear so as to not get HOM effects
		float clear[4] = {0.1f, 0.1f, 0.1f, 0.0f};
		d3d_Context->lpVtbl->ClearRenderTargetView (d3d_Context, RTV, clear);
	}
	else if (gl_clear->value)
	{
		float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		d3d_Context->lpVtbl->ClearRenderTargetView (d3d_Context, RTV, clear);
	}

	// standard depth clear
	d3d_Context->lpVtbl->ClearDepthStencilView (d3d_Context, DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 1);
}


void R_SyncPipeline (void)
{
	// forces the pipeline to sync by issuing a query then waiting for it to complete
	ID3D11Query *FinishEvent = NULL;
	D3D11_QUERY_DESC Desc = {D3D11_QUERY_EVENT, 0};

	if (SUCCEEDED (d3d_Device->lpVtbl->CreateQuery (d3d_Device, &Desc, &FinishEvent)))
	{
		d3d_Context->lpVtbl->End (d3d_Context, (ID3D11Asynchronous *) FinishEvent);
		while (d3d_Context->lpVtbl->GetData (d3d_Context, (ID3D11Asynchronous *) FinishEvent, NULL, 0, 0) == S_FALSE);
		SAFE_RELEASE (FinishEvent);
	}
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (v_blend[3] > 0)
	{
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSNoCull);
		D_BindShaderBundle (d3d_PolyblendShader);

		// full-screen triangle
		d3d_Context->lpVtbl->Draw (d3d_Context, 3, 0);
	}
}


void R_PrepareEntities (void)
{
	int		i;

	if (!r_drawentities->value)
		return;

	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		entity_t *e = &r_newrefdef.entities[i];

		if (r_testnullmodels->value && !(e->flags & RF_WEAPONMODEL))
		{
			// hack for testing NULL models
			e->model = NULL;
		}

		if (e->flags & RF_BEAM)
		{
			R_PrepareBeam (e, &r_local_matrix[i]);
		}
		else
		{
			if (!e->model)
			{
				R_PrepareNullModel (e, &r_local_matrix[i]);
				continue;
			}

			switch (e->model->type)
			{
			case mod_alias:
				R_PrepareAliasModel (e, &r_local_matrix[i]);
				break;

			case mod_brush:
				R_PrepareBrushModel (e, &r_local_matrix[i]);
				break;

			case mod_sprite:
				R_PrepareSpriteModel (e, &r_local_matrix[i]);
				break;

			case mod_md5:
				R_PrepareMD5Model (e, &r_local_matrix[i]);
				break;

			default:
				ri.Sys_Error (ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}
}


void R_RenderScene (void)
{
	R_DrawWorld ();

	R_DrawEntitiesOnList (false);

	// we could depth-sort alpha objects but for now we're working on the assumption that original Q2 content was designed around it's engine
	// behaviours, and trying to change as little of that as possible.
	R_DrawEntitiesOnList (true);

	// switch the drawing order if under vs above water so things will look approximately correct
	if (r_newrefdef.rdflags & RDF_UNDERWATER)
	{
		R_DrawParticles ();
		R_DrawAlphaSurfaces ();
	}
	else
	{
		R_DrawAlphaSurfaces ();
		R_DrawParticles ();
	}
}


/*
====================
R_RenderFrame

====================
*/
void R_RenderFrame (refdef_t *fd)
{
	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
		ri.Sys_Error (ERR_DROP, "R_RenderFrame: NULL worldmodel");

	// check and recreate everything that needs to be done once per frame
	R_LightFrame ();

	// set up to draw
	if (gl_finish->value)
		R_SyncPipeline ();

	R_SetupFrame ();

	R_SetupGL ();

	// prepare everything that will be drawn
	R_MarkLeaves ();	// done here so we know if we're in water

	R_PrepareDlights ();

	R_PrepareEntities ();

	// draw it
	if (D_BeginWaterWarp ())
	{
		R_RenderScene ();
		D_DoWaterWarp ();
	}
	else
	{
		R_RenderScene ();
		R_PolyBlend ();
	}

	// for sending client light level back to the server
	R_SetLightLevel ();
}


