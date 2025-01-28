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
// gl_mesh.c: triangle model functions

#include "r_local.h"
#include "mesh.h"

aliasbuffers_t d3d_AliasBuffers[MAX_MOD_KNOWN];

void R_ShutdownMesh (void)
{
	for (int i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		SAFE_RELEASE (set->PolyVerts);
		SAFE_RELEASE (set->TexCoords);
		SAFE_RELEASE (set->Indexes);

		memset (set, 0, sizeof (aliasbuffers_t));
	}
}


void R_FreeUnusedAliasBuffers (void)
{
	for (int i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		if (set->registration_sequence != r_registration_sequence)
		{
			SAFE_RELEASE (set->PolyVerts);
			SAFE_RELEASE (set->TexCoords);
			SAFE_RELEASE (set->Indexes);

			memset (set, 0, sizeof (aliasbuffers_t));
		}
	}
}


aliasbuffers_t *R_GetBufferSetForIndex (int index)
{
	return &d3d_AliasBuffers[index];
}


int D_FindAliasBuffers (model_t *mod)
{
	// see do we already have it
	for (int i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		if (!set->PolyVerts) continue;
		if (!set->TexCoords) continue;
		if (!set->Indexes) continue;

		if (strcmp (set->Name, mod->name)) continue;

		// use this set and mark it as active
		mod->bufferset = i;
		set->registration_sequence = r_registration_sequence;

		return mod->bufferset;
	}

	return -1; // not found
}


void D_RegisterAliasBuffers (model_t *mod)
{
	// see do we already have it
	if ((mod->bufferset = D_FindAliasBuffers (mod)) != -1) return;
	ri.Sys_Error (ERR_DROP, "D_RegisterAliasBuffers : buffer set for %s was not created", mod->name);
}


int D_GetFreeBufferSet (void)
{
	// find the first free buffer
	for (int i = 0; i < MAX_MOD_KNOWN; i++)
	{
		aliasbuffers_t *set = &d3d_AliasBuffers[i];

		// already allocated
		if (set->PolyVerts) continue;
		if (set->TexCoords) continue;
		if (set->Indexes) continue;

		// found a free one
		return i;
	}

	// not found
	return -1;
}


/*
==================
Mesh_BuildFrameNormals

==================
*/
void Mesh_BuildFrameNormals (meshpolyvert_t *verts, int numverts, const mesh_triangle_t *triangles, int numtris, vertexnormals_t *vnorms)
{
	int i, j;

	// no normals initially - we can't do dst++ here as we need to preserve the pointer for the next step
	for (i = 0; i < numverts; i++)
	{
		// now initialize it
		vnorms[i].normal[0] = vnorms[i].normal[1] = vnorms[i].normal[2] = 0;
		vnorms[i].numnormals = 0;
	}

	// accumulate triangle normals to vnorms
	for (i = 0; i < numtris; i++)
	{
		float triverts[3][3];
		float vtemp1[3], vtemp2[3], normal[3];

		// undo the vertex rotation from modelgen.c here (consistent with modelgen.c)
		for (j = 0; j < 3; j++)
		{
			triverts[j][0] = verts[triangles[i].index[j]].position[1];
			triverts[j][1] = -verts[triangles[i].index[j]].position[0];
			triverts[j][2] = verts[triangles[i].index[j]].position[2];
		}

		// calc the per-triangle normal
		Vector3Subtract (vtemp1, triverts[0], triverts[1]);
		Vector3Subtract (vtemp2, triverts[2], triverts[1]);
		Vector3Cross (normal, vtemp1, vtemp2);
		Vector3Normalize (normal);

		// and accumulate it into the calculated normals array
		for (j = 0; j < 3; j++)
		{
			// rotate the normal so the model faces down the positive x axis (consistent with modelgen.c)
			vnorms[triangles[i].index[j]].normal[0] -= normal[1];
			vnorms[triangles[i].index[j]].normal[1] += normal[0];
			vnorms[triangles[i].index[j]].normal[2] += normal[2];

			// count the normals for averaging
			vnorms[triangles[i].index[j]].numnormals++;
		}
	}

	// calculate final normals - the frame is already in the correct space so nothing further needs to be done here
	for (i = 0; i < numverts; i++)
	{
		// numnormals was checked for > 0 in modelgen.c so we shouldn't need to do it again here but we do anyway just in case a rogue modder has used a bad modelling tool
		if (vnorms[i].numnormals > 0)
		{
			Vector3Scalef (vnorms[i].normal, vnorms[i].normal, (float) vnorms[i].numnormals);
			Vector3Normalize (vnorms[i].normal);
		}
		else
		{
			vnorms[i].normal[0] = vnorms[i].normal[1] = 0;
			vnorms[i].normal[2] = 1;
		}

		verts[i].normal[0] = vnorms[i].normal[0];
		verts[i].normal[1] = vnorms[i].normal[1];
		verts[i].normal[2] = vnorms[i].normal[2];
	}
}


