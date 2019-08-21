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


static void D_CreateSpriteBufferSet (model_t *mod, dsprite_t *psprite)
{
	int i;
	spritebuffers_t *set = &d3d_SpriteBuffers[mod->bufferset];
	spritepolyvert_t *verts = (spritepolyvert_t *) ri.Load_AllocMemory (sizeof (spritepolyvert_t) * 4 * psprite->numframes);

	D3D11_BUFFER_DESC vbDesc = {
		sizeof (spritepolyvert_t) * 4 * psprite->numframes,
		D3D11_USAGE_IMMUTABLE,
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
		D_CreateSpriteBufferSet (mod, mod->sprheader);

		// and done
		return;
	}

	ri.Sys_Error (ERR_DROP, "D_MakeSpriteBuffers : not enough free buffers!");
}


int d3d_SpriteShader;
ID3D11Buffer *d3d_SpriteIndexes;

void R_InitSprites (void)
{
	// it's kinda slightly sucky that we need to create a new index buffer just for this.....
	unsigned short indexes[6] = {0, 1, 2, 0, 2, 3};

	D3D11_BUFFER_DESC ibDesc = {
		sizeof (indexes),
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	D3D11_SUBRESOURCE_DATA srd = {indexes, 0, 0};
	D3D11_INPUT_ELEMENT_DESC layout[] = {VDECL ("XYOFFSET", 0, DXGI_FORMAT_R32G32_FLOAT, 5, 0)};

	d3d_SpriteShader = D_CreateShaderBundle (IDR_SPRITESHADER, "SpriteVS", NULL, "SpritePS", DEFINE_LAYOUT (layout));

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &d3d_SpriteIndexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_SpriteIndexes, "d3d_SpriteIndexes");
}


void R_ShutdownSprites (void)
{
	int i;

	for (i = 0; i < MAX_MOD_KNOWN; i++)
	{
		spritebuffers_t *set = &d3d_SpriteBuffers[i];
		SAFE_RELEASE (set->PolyVerts);
		memset (set, 0, sizeof (spritebuffers_t));
	}
}


void R_DrawSpriteModel (entity_t *e, QMATRIX *localmatrix)
{
	model_t *mod = e->model;

	// don't even bother culling, because it's just a single polygon without a surface cache
	// (note - with hardware it might make sense to cull)
	dsprite_t *psprite = mod->sprheader;
	int framenum = e->currframe % psprite->numframes;
	dsprframe_t *frame = &psprite->frames[framenum];

	// because there's buffer, shader and texture changes, as well as cbuffer updates, it's worth spending some CPU
	// time on cullboxing sprites
	float mins[3] = {
		e->currorigin[0] - frame->origin_x,
		e->currorigin[1] - frame->origin_y,
		e->currorigin[2] - 1 // so that the box isn't a 2d plane...
	};

	float maxs[3] = {
		e->currorigin[0] + frame->width - frame->origin_x,
		e->currorigin[1] + frame->height - frame->origin_y,
		e->currorigin[2] + 1 // so that the box isn't a 2d plane...
	};

	if (R_CullBox (mins, maxs)) return;

	R_PrepareEntityForRendering (localmatrix, NULL, e->alpha, e->flags);
	D_BindShaderBundle (d3d_SpriteShader);
	R_BindTexture (mod->skins[framenum]->SRV);

	D_BindVertexBuffer (5, d3d_SpriteBuffers[mod->bufferset].PolyVerts, sizeof (spritepolyvert_t), 0);
	D_BindIndexBuffer (d3d_SpriteIndexes, DXGI_FORMAT_R16_UINT);

	d3d_Context->lpVtbl->DrawIndexed (d3d_Context, 6, 0, framenum * 4);
}


void R_PrepareSpriteModel (entity_t *e, QMATRIX *localmatrix)
{
	// get the transform in local space so that we can correctly handle dlights
	R_MatrixIdentity (localmatrix);
	R_MatrixTranslate (localmatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
}


