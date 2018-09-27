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


typedef struct spriteconstants_s {
	float spriteOrigin[3];
	float spritealpha;
} spriteconstants_t;

typedef struct spritepolyvert_s {
	float XYOffset[2];
} spritepolyvert_t;

typedef struct spritebuffers_s {
	ID3D11Buffer *PolyVerts;
	char Name[256];
	int registration_sequence;
} spritebuffers_t;

static spritebuffers_t d3d_SpriteBuffers[MAX_MOD_KNOWN];


void R_FreeUnusedSpriteBuffers (void)
{
	int i;

	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		spritebuffers_t *set = &d3d_SpriteBuffers[i];

		if (set->registration_sequence != r_registration_sequence)
		{
			SAFE_RELEASE (set->PolyVerts);
			memset (set, 0, sizeof (spritebuffers_t));
		}
	}
}


void D_CreateSpriteBufferSet (model_t *mod, dsprite_t *psprite)
{
	int i;
	spritebuffers_t *set = &d3d_SpriteBuffers[mod->bufferset];
	spritepolyvert_t *verts = (spritepolyvert_t *) ri.Load_AllocMemory (sizeof (spritepolyvert_t) * 4 * psprite->numframes);

	D3D11_BUFFER_DESC vbDesc = {
		sizeof (spritepolyvert_t) * 4 * psprite->numframes,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// alloc a buffer to write the verts to and create the VB from
	D3D11_SUBRESOURCE_DATA srd = {verts, 0, 0};

	// fill in the verts
	for (i = 0; i < psprite->numframes; i++, verts += 4)
	{
		dsprframe_t *frame = &psprite->frames[i];

		// fixme - move these to SV_VertexID based as well
		verts[0].XYOffset[0] = -frame->origin_y;
		verts[0].XYOffset[1] = -frame->origin_x;

		verts[1].XYOffset[0] = frame->height - frame->origin_y;
		verts[1].XYOffset[1] = -frame->origin_x;

		verts[2].XYOffset[0] = frame->height - frame->origin_y;
		verts[2].XYOffset[1] = frame->width - frame->origin_x;

		verts[3].XYOffset[0] = -frame->origin_y;
		verts[3].XYOffset[1] = frame->width - frame->origin_x;
	}

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->PolyVerts);
	ri.Load_FreeMemory ();
}


int D_FindSpriteBuffers (model_t *mod)
{
	int i;

	// see do we already have it
	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		spritebuffers_t *set = &d3d_SpriteBuffers[i];

		if (!set->PolyVerts) continue;
		if (strcmp (set->Name, mod->name)) continue;

		// use this set and mark it as active
		mod->bufferset = i;
		set->registration_sequence = r_registration_sequence;

		return mod->bufferset;
	}

	return -1;
}


void D_MakeSpriteBuffers (model_t *mod)
{
	int i;

	// see do we already have it
	if ((mod->bufferset = D_FindSpriteBuffers (mod)) != -1) return;

	// find the first free buffer
	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		spritebuffers_t *set = &d3d_SpriteBuffers[i];

		// already allocated
		if (set->PolyVerts) continue;

		// cache the name so that we'll find it next time too
		strcpy (set->Name, mod->name);

		// use this set and mark it as active
		mod->bufferset = i;
		set->registration_sequence = r_registration_sequence;

		// now build everything from the model data
		D_CreateSpriteBufferSet (mod, (dsprite_t *) mod->extradata);

		// and done
		return;
	}

	ri.Sys_Error (ERR_DROP, "D_MakeSpriteBuffers : not enough free buffers!");
}


int d3d_SpriteShader;

ID3D11Buffer *d3d_SpriteConstants;
ID3D11Buffer *d3d_SpriteIndexes;

void R_InitSprites (void)
{
	unsigned short indexes[6] = {0, 1, 2, 0, 2, 3};

	D3D11_BUFFER_DESC ibDesc = {
		sizeof (indexes),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	D3D11_BUFFER_DESC cbPerSpriteDesc = {
		sizeof (spriteconstants_t),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_CONSTANT_BUFFER,
		0,
		0,
		0
	};

	D3D11_SUBRESOURCE_DATA srd = {indexes, 0, 0};
	D3D11_INPUT_ELEMENT_DESC layout[] = {VDECL ("XYOFFSET", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0)};

	d3d_SpriteShader = D_CreateShaderBundle (IDR_MISCSHADER, "SpriteVS", NULL, "SpritePS", DEFINE_LAYOUT (layout));

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &d3d_SpriteIndexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_SpriteIndexes, "d3d_SpriteIndexes");

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &cbPerSpriteDesc, NULL, &d3d_SpriteConstants);
	D_RegisterConstantBuffer (d3d_SpriteConstants, 6);
}


void R_ShutdownSprite (void)
{
	int i;

	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		spritebuffers_t *set = &d3d_SpriteBuffers[i];
		SAFE_RELEASE (set->PolyVerts);
		memset (set, 0, sizeof (spritebuffers_t));
	}
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	model_t *mod = e->model;

	// don't even bother culling, because it's just a single polygon without a surface cache
	// (note - widht hardware it might make sense to cull)
	dsprite_t *psprite = (dsprite_t *) mod->extradata;
	int framenum = e->currframe % psprite->numframes;
	dsprframe_t *frame = &psprite->frames[framenum];

	spriteconstants_t consts;

	Vector3Copy (consts.spriteOrigin, e->currorigin);

	if (e->flags & RF_TRANSLUCENT)
		consts.spritealpha = e->alpha;
	else consts.spritealpha = 1;

	d3d_Context->lpVtbl->UpdateSubresource (d3d_Context, (ID3D11Resource *) d3d_SpriteConstants, 0, NULL, &consts, 0, 0);

	D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSFullCull);
	D_BindShaderBundle (d3d_SpriteShader);
	R_BindTexture (mod->skins[framenum]->SRV);

	D_BindVertexBuffer (0, d3d_SpriteBuffers[mod->bufferset].PolyVerts, sizeof (spritepolyvert_t), 0);
	D_BindIndexBuffer (d3d_SpriteIndexes, DXGI_FORMAT_R16_UINT);

	d3d_Context->lpVtbl->DrawIndexed (d3d_Context, 6, 0, framenum * 4);
}


