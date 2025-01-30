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

#define VERTEXSLOT_BEAM		7

typedef struct beampolyvert_s {
	// this exists so that i don't get confused over what the actual count of verts is
	// because "* 3" for number of floats in a vert vs "* 3" for number of verts in a triangle caused me to come a cropper with this code before
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
		D3D11_USAGE_IMMUTABLE,
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
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	unsigned short *indexes = (unsigned short *) ri.Hunk_Alloc (r_numbeamindexes * sizeof (unsigned short));
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

	int mark = ri.Hunk_LowMark ();

	// clamp sensibly
	if (slices < 3) slices = 3;
	if (slices > 360) slices = 360;

	r_numbeamverts = (slices + 1) * 2;
	r_numbeamindexes = (r_numbeamverts - 2) * 3;
	verts = (beampolyvert_t *) ri.Hunk_Alloc (r_numbeamverts * sizeof (beampolyvert_t));

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
	ri.Hunk_FreeToLowMark (mark);
}


static int d3d_BeamShader = 0;

void R_InitBeam (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, VERTEXSLOT_BEAM, 0)
	};

	d3d_BeamShader = D_CreateShaderBundle (IDR_BEAMSHADER, "BeamVS", NULL, "BeamPS", DEFINE_LAYOUT (layout));

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

		D_BindVertexBuffer (VERTEXSLOT_BEAM, d3d_BeamVertexes, sizeof (beampolyvert_t), 0);
		D_BindIndexBuffer (d3d_BeamIndexes, DXGI_FORMAT_R16_UINT);

		d3d_Context->lpVtbl->DrawIndexed (d3d_Context, r_numbeamindexes, 0, 0);
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


void R_BeamFrame (void)
{
	if (r_beamdetail->modified)
	{
		R_ShutdownBeam ();
		R_CreateBeamVertexes (r_beamdetail->value);
		r_beamdetail->modified = false;
	}
}


