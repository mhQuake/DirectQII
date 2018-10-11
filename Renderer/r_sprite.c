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
// r_misc.c

#include "r_local.h"


__declspec(align(16)) typedef struct spriteconstants_s {
	float xywh[4];
} spriteconstants_t;


int d3d_SpriteShader;
ID3D11Buffer *d3d_SpriteConstants;


void R_InitSprites (void)
{
	D3D11_BUFFER_DESC cbPerSpriteDesc = {
		sizeof (spriteconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	d3d_SpriteShader = D_CreateShaderBundle (IDR_MISCSHADER, "SpriteVS", "SpriteGS", "SpritePS", NULL, 0);

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbPerSpriteDesc, NULL, &d3d_SpriteConstants);
	D_RegisterConstantBuffer (d3d_SpriteConstants, 5);
}


void R_UpdateSpriteConstants (entity_t *e, dsprframe_t *frame)
{
	spriteconstants_t consts;

	consts.xywh[0] = -frame->origin_x;
	consts.xywh[1] = -frame->origin_y;
	consts.xywh[2] = frame->width - frame->origin_x;
	consts.xywh[3] = frame->height - frame->origin_y;

	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_SpriteConstants, 0, NULL, &consts, 0, 0);
}


void R_DrawSpriteModel (entity_t *e, QMATRIX *localmatrix)
{
	model_t *mod = e->model;

	// don't even bother culling, because it's just a single polygon without a surface cache
	// (note - with hardware it might make sense to cull)
	dsprite_t *psprite = mod->sprheader;
	int framenum = e->currframe % psprite->numframes;
	dsprframe_t *frame = &psprite->frames[framenum];

	R_UpdateSpriteConstants (e, frame);

	R_PrepareEntityForRendering (localmatrix, NULL, e->alpha, e->flags);
	D_BindShaderBundle (d3d_SpriteShader);
	R_BindTexture (mod->skins[framenum]->SRV);

	// we'll do triangle-to-quad expansion in our geometry shader which is hellishly cute
	d3d_Context->lpVtbl->Draw (d3d_Context, 3, 0);
}


void R_PrepareSpriteModel (entity_t *e, QMATRIX *localmatrix)
{
	// get the transform in local space so that we can correctly handle dlights
	R_MatrixIdentity (localmatrix);
	R_MatrixTranslate (localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
}


