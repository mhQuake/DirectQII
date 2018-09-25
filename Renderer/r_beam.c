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


typedef struct beampolyvert_s {
	float position[3];
} beampolyvert_t;

int r_numbeamverts = 0;
int r_numbeamindexes = 0;

ID3D11Buffer *d3d_BeamVertexes = NULL;
ID3D11Buffer *d3d_BeamIndexes = NULL;

void R_CreateBeamVertexBuffer (D3D11_SUBRESOURCE_DATA *vbSrd)
{
	D3D11_BUFFER_DESC vbDesc = {
		r_numbeamverts * sizeof (beampolyvert_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, vbSrd, &d3d_BeamVertexes);
}


void R_CreateBeamIndexBuffer (void)
{
	D3D11_BUFFER_DESC ibDesc = {
		r_numbeamindexes * sizeof (unsigned short),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	unsigned short *indexes = (unsigned short *) ri.Load_AllocMemory (r_numbeamindexes * sizeof (unsigned short));
	unsigned short *ndx = indexes;

	D3D11_SUBRESOURCE_DATA srd = {indexes, 0, 0};

	int i, numindexes = 0;

	for (i = 2; i < r_numbeamverts; i++, ndx += 3, numindexes += 3)
	{
		ndx[0] = i - 2;
		ndx[1] = (i & 1) ? i : (i - 1);
		ndx[2] = (i & 1) ? (i - 1) : i;
	}

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &d3d_BeamIndexes);
}


void R_CreateBeamVertexes (int slices)
{
	int i;
	beampolyvert_t *verts = NULL;
	D3D11_SUBRESOURCE_DATA srd;

	// clamp sensibly
	if (slices < 3) slices = 3;
	if (slices > 360) slices = 360;

	r_numbeamverts = (slices + 1) * 2;
	r_numbeamindexes = (r_numbeamverts - 2) * 3;
	verts = (beampolyvert_t *) ri.Load_AllocMemory (r_numbeamverts * sizeof (beampolyvert_t));

	srd.pSysMem = verts;
	srd.SysMemPitch = 0;
	srd.SysMemSlicePitch = 0;

	for (i = 0; i <= slices; i++, verts += 2)
	{
		float angle = (i == slices) ? 0.0f : (2.0f * M_PI * (float) i / (float) slices);

		Vector3Set (verts[0].position, sin (angle) * 0.5f, cos (angle) * 0.5f, 1);
		Vector3Set (verts[1].position, sin (angle) * 0.5f, cos (angle) * 0.5f, 0);
	}

	// create the buffers from the generated data
	R_CreateBeamVertexBuffer (&srd);
	R_CreateBeamIndexBuffer ();

	// throw away memory used for loading
	ri.Load_FreeMemory ();
}


static int d3d_BeamShader = 0;

void R_InitBeam (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0)
	};

	d3d_BeamShader = D_CreateShaderBundle (IDR_MISCSHADER, "BeamVS", NULL, "BeamPS", DEFINE_LAYOUT (layout));

	if (r_beamdetail)
	{
		R_CreateBeamVertexes (r_beamdetail->value);
		r_beamdetail->modified = false; // don't trigger unnecessarily
	}
	else R_CreateBeamVertexes (24);
}


void R_ShutdownBeam (void)
{
	SAFE_RELEASE (d3d_BeamVertexes);
	SAFE_RELEASE (d3d_BeamIndexes);
}


/*
=============
R_DrawBeam
=============
*/
void R_DrawBeam (entity_t *e)
{
	int i;
	float mins[3] = {999999, 999999, 999999};
	float maxs[3] = {-999999, -999999, -999999};
	float dir[3], len, axis[3], color[3], upvec[3] = {0, 0, 1};
	QMATRIX localmatrix;

	for (i = 0; i < 3; i++)
	{
		if (e->currorigin[i] < mins[i]) mins[i] = e->currorigin[i];
		if (e->currorigin[i] > maxs[i]) maxs[i] = e->currorigin[i];
		if (e->prevorigin[i] < mins[i]) mins[i] = e->prevorigin[i];
		if (e->prevorigin[i] > maxs[i]) maxs[i] = e->prevorigin[i];
	}

	// currframe is a radius, so halve it (rounding up in case it's odd)
	// we don't know the full orientation of the beam so just adjust all directions
	Vector3Subtractf (mins, mins, (e->currframe + 1) / 2);
	Vector3Addf (maxs, maxs, (e->currframe + 1) / 2);

	if (R_CullBox (mins, maxs)) return;

	R_MatrixIdentity (&localmatrix);

	Vector3Subtract (dir, e->prevorigin, e->currorigin);
	Vector3Cross (axis, upvec, dir);

	// catch 0 length beams
	if ((len = Vector3Length (dir)) < 0.000001f) return;

	// and transform it
	R_MatrixTranslate (&localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotateAxis (&localmatrix, (180.0f / M_PI) * acos ((Vector3Dot (upvec, dir) / len)), axis[0], axis[1], axis[2]);
	R_MatrixScale (&localmatrix, e->currframe, e->currframe, len);

	color[0] = (float) ((byte *) &d_8to24table[e->skinnum & 0xff])[0] / 255.0f;
	color[1] = (float) ((byte *) &d_8to24table[e->skinnum & 0xff])[1] / 255.0f;
	color[2] = (float) ((byte *) &d_8to24table[e->skinnum & 0xff])[2] / 255.0f;

	if (e->alpha < 1)
	{
		R_UpdateAlpha (e->alpha);
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSFullCull);
	}
	else
	{
		R_UpdateAlpha (1);
		D_SetRenderStates (d3d_BSNone, d3d_DSFullDepth, d3d_RSFullCull);
	}

	R_UpdateEntityConstants (&localmatrix, color, 0);
	D_BindShaderBundle (d3d_BeamShader);

	if (r_beamdetail->modified)
	{
		R_ShutdownBeam ();
		R_CreateBeamVertexes (r_beamdetail->value);
		r_beamdetail->modified = false;
	}

	D_BindVertexBuffer (0, d3d_BeamVertexes, sizeof (beampolyvert_t), 0);
	D_BindIndexBuffer (d3d_BeamIndexes, DXGI_FORMAT_R16_UINT);

	d3d_Context->lpVtbl->DrawIndexed (d3d_Context, r_numbeamindexes, 0, 0);
}

