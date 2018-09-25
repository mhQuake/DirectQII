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


typedef struct nullpolyvert_s {
	float xyz[3];
	float normal[3];
} nullpolyvert_t;

ID3D11Buffer *d3d_NullVertexes = NULL;
int d3d_NullShader = 0;
int d3d_NumNullVerts = 0;

void R_InitNull (void)
{
	// drawn as a trilist because we need individual triangle normals
	nullpolyvert_t nullverts[] = {
		{{0.000000, 0.000000, -1.000000}, {0.577350, 0.577350, -0.577350}}, {{1.000000, 0.000000, 0.000000}, {0.577350, 0.577350, -0.577350}}, 
		{{0.000000, 1.000000, 0.000000}, {0.577350, 0.577350, -0.577350}}, {{0.000000, 0.000000, -1.000000}, {-0.577350, 0.577350, -0.577350}},
		{{0.000000, 1.000000, 0.000000}, {-0.577350, 0.577350, -0.577350}}, {{-1.000000, 0.000000, 0.000000}, {-0.577350, 0.577350, -0.577350}},
		{{0.000000, 0.000000, -1.000000}, {-0.577350, -0.577350, -0.577350}}, {{-1.000000, 0.000000, 0.000000}, {-0.577350, -0.577350, -0.577350}},
		{{0.000000, -1.000000, 0.000000}, {-0.577350, -0.577350, -0.577350}}, {{0.000000, 0.000000, -1.000000}, {0.577350, -0.577350, -0.577350}},
		{{0.000000, -1.000000, 0.000000}, {0.577350, -0.577350, -0.577350}}, {{1.000000, 0.000000, 0.000000}, {0.577350, -0.577350, -0.577350}},
		{{0.000000, 0.000000, 1.000000}, {0.577350, -0.577350, 0.577350}}, {{1.000000, 0.000000, 0.000000}, {0.577350, -0.577350, 0.577350}},
		{{0.000000, -1.000000, 0.000000}, {0.577350, -0.577350, 0.577350}}, {{0.000000, 0.000000, 1.000000}, {-0.577350, -0.577350, 0.577350}},
		{{0.000000, -1.000000, 0.000000}, {-0.577350, -0.577350, 0.577350}}, {{-1.000000, 0.000000, 0.000000}, {-0.577350, -0.577350, 0.577350}},
		{{0.000000, 0.000000, 1.000000}, {-0.577350, 0.577350, 0.577350}}, {{-1.000000, 0.000000, 0.000000}, {-0.577350, 0.577350, 0.577350}},
		{{0.000000, 1.000000, 0.000000}, {-0.577350, 0.577350, 0.577350}}, {{0.000000, 0.000000, 1.000000}, {0.577350, 0.577350, 0.577350}},
		{{0.000000, 1.000000, 0.000000}, {0.577350, 0.577350, 0.577350}}, {{1.000000, 0.000000, 0.000000}, {0.577350, 0.577350, 0.577350}}
	};

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0),
		VDECL ("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0)
	};

	D3D11_BUFFER_DESC vbDesc = {
		sizeof (nullverts),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	D3D11_SUBRESOURCE_DATA srd = {nullverts, 0, 0};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &d3d_NullVertexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_NullVertexes, "d3d_NullVertexes");

	d3d_NumNullVerts = sizeof (nullverts) / sizeof (nullverts[0]);
	d3d_NullShader = D_CreateShaderBundle (IDR_MISCSHADER, "NullVS", NULL, "NullPS", DEFINE_LAYOUT (layout));
}


/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (entity_t *e)
{
	float shadelight[3];
	QMATRIX localmatrix;

	if (e->flags & RF_FULLBRIGHT)
		Vector3Set (shadelight, 1, 1, 1);
	else
	{
		R_LightPoint (e->currorigin, shadelight);
		R_DynamicLightPoint (e->currorigin, shadelight);
	}

	R_MatrixIdentity (&localmatrix);
	R_MatrixTranslate (&localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	R_MatrixRotate (&localmatrix, -e->angles[0], e->angles[1], -e->angles[2]);

	if (e->flags & RF_TRANSLUCENT)
	{
		R_UpdateAlpha (e->alpha);
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSFullCull);
	}
	else
	{
		R_UpdateAlpha (1);
		D_SetRenderStates (d3d_BSNone, d3d_DSFullDepth, d3d_RSFullCull);
	}

	R_UpdateEntityConstants (&localmatrix, shadelight, 0);
	D_BindVertexBuffer (0, d3d_NullVertexes, sizeof (nullpolyvert_t), 0);
	D_BindShaderBundle (d3d_NullShader);

	d3d_Context->lpVtbl->Draw (d3d_Context, d3d_NumNullVerts, 0);
}

