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

#include "r_local.h"


static int d3d_BeamShader = 0;

void R_InitBeam (void)
{
	d3d_BeamShader = D_CreateShaderBundle (IDR_BEAMSHADER, "BeamVS", NULL, "BeamPS", NULL, 0);
}


int R_CreateBeamVertexes (int slices)
{
	int r_numbeamverts = 0;

	// clamp sensibly
	// this should match the detail setup in R_SetupGL
	if (slices < 3) slices = 3;
	if (slices > 360) slices = 360;

	// figure counts
	r_numbeamverts = (slices + 1) * 2;

	return r_numbeamverts;
}


void R_DrawBeam (entity_t *e, QMATRIX *localmatrix)
{
	float mins[3] = {-0.5f, -0.5f, 0.0f};
	float maxs[3] = {0.5f, 0.5f, 1.0f};

	// don't draw fully translucent beams
	if (!(e->alpha > 0))
		return;
	else if (R_CullForEntity (mins, maxs, localmatrix))
		return;
	else
	{
		float color[3] = {
			(float) ((byte *) &d_8to24table_solid[e->skinnum & 0xff])[0] / 255.0f,
			(float) ((byte *) &d_8to24table_solid[e->skinnum & 0xff])[1] / 255.0f,
			(float) ((byte *) &d_8to24table_solid[e->skinnum & 0xff])[2] / 255.0f
		};

		R_PrepareEntityForRendering (localmatrix, color, e->alpha, RF_TRANSLUCENT);
		D_BindShaderBundle (d3d_BeamShader);

		// go into tristrip mode
		d3d_Context->lpVtbl->IASetPrimitiveTopology (d3d_Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		// draw at the requested detail level
		d3d_Context->lpVtbl->Draw (d3d_Context, R_CreateBeamVertexes ((int) r_beamdetail->value), 0);

		// back to trilist mode
		d3d_Context->lpVtbl->IASetPrimitiveTopology (d3d_Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}


void R_PrepareBeam (entity_t *e, QMATRIX *localmatrix)
{
	float dir[3], len, axis[3], upvec[3] = {0, 0, 1};

	Vector3Subtract (dir, e->prevorigin, e->currorigin);
	Vector3Cross (axis, upvec, dir);
	Vector3Normalize (axis);

	// catch 0 length beams
	if (!((len = Vector3Length (dir)) > 0))
	{
		// hack to not draw it
		e->alpha = 0;
		return;
	}

	// and transform it (there really should be a better way of doing this)
	R_MatrixIdentity (localmatrix);
	R_MatrixTranslate (localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotateAxis (localmatrix, (180.0f / M_PI) * acos ((Vector3Dot (upvec, dir) / len)), axis[0], axis[1], axis[2]);
	R_MatrixScale (localmatrix, e->currframe, e->currframe, len);
}

