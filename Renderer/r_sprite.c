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


int d3d_SpriteShader;

void R_InitSprites (void)
{
	d3d_SpriteShader = D_CreateShaderBundleForQuadBatch (IDR_MISCSHADER, "SpriteVS", "SpritePS");
}


void D_DrawSpriteModel (float positions[4][3], int alpha, ID3D11ShaderResourceView *SRV)
{
	D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSDepthNoWrite, d3d_RSFullCull);
	D_BindShaderBundle (d3d_SpriteShader);
	GL_BindTexture (SRV);

	D_CheckQuadBatch ();

	D_QuadVertexPosition3fvColorTexCoord2f (positions[0], 0xffffff | (alpha << 24), 0, 1);
	D_QuadVertexPosition3fvColorTexCoord2f (positions[1], 0xffffff | (alpha << 24), 0, 0);
	D_QuadVertexPosition3fvColorTexCoord2f (positions[2], 0xffffff | (alpha << 24), 1, 0);
	D_QuadVertexPosition3fvColorTexCoord2f (positions[3], 0xffffff | (alpha << 24), 1, 1);

	D_EndQuadBatch ();
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	float	positions[4][3];
	model_t *mod = e->model;

	// don't even bother culling, because it's just a single polygon without a surface cache
	dsprite_t *psprite = (dsprite_t *) mod->extradata;
	int framenum = e->currframe % psprite->numframes;
	dsprframe_t *frame = &psprite->frames[framenum];

	VectorMA (e->currorigin, -frame->origin_y, vup, positions[0]);
	VectorMA (positions[0], -frame->origin_x, vright, positions[0]);

	VectorMA (e->currorigin, frame->height - frame->origin_y, vup, positions[1]);
	VectorMA (positions[1], -frame->origin_x, vright, positions[1]);

	VectorMA (e->currorigin, frame->height - frame->origin_y, vup, positions[2]);
	VectorMA (positions[2], frame->width - frame->origin_x, vright, positions[2]);

	VectorMA (e->currorigin, -frame->origin_y, vup, positions[3]);
	VectorMA (positions[3], frame->width - frame->origin_x, vright, positions[3]);

	if (e->flags & RF_TRANSLUCENT)
	{
		if (e->alpha < 0)
			return;
		else if (e->alpha < 1)
			D_DrawSpriteModel (positions, (int) (e->alpha * 255), mod->skins[framenum]->SRV);
		else D_DrawSpriteModel (positions, 255, mod->skins[framenum]->SRV);
	}
	else D_DrawSpriteModel (positions, 255, mod->skins[framenum]->SRV);
}


