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

qboolean VCache_ReorderIndices (char *name, unsigned short *outIndices, const unsigned short *indices, int nTriangles, int nVertices);
void VCache_Init (void);
void R_GetVertexNormal (float *normal, int index);

// deduplication
typedef struct aliasmesh_s {
	short index_xyz;
	short index_st;
} aliasmesh_t;

// Triangle
typedef struct mesh_triangle_s {
	unsigned short index[3];
} mesh_triangle_t;

// vertex normals calculation
typedef struct vertexnormals_s {
	int numnormals;
	float normal[3];
} vertexnormals_t;

typedef struct aliasbuffers_s {
	ID3D11Buffer *PolyVerts;
	ID3D11Buffer *TexCoords;
	ID3D11Buffer *Indexes;

	char Name[256];
	int registration_sequence;

	// for drawing from
	int framevertexstride;
	int numindexes;
} aliasbuffers_t;

__declspec(align(16)) typedef struct meshconstants_s {
	float shadelight[4];	// padded for cbuffer
	float shadevector[4];	// padded for cbuffer
	float move[4];			// padded for cbuffer
	float frontv[4];		// padded for cbuffer
	float backv[4];			// padded for cbuffer
	float suitscale;
	float backlerp;
	float junk[2];			// cbuffer padding
} meshconstants_t;


aliasbuffers_t *R_GetBufferSetForIndex (int index);
void D_RegisterAliasBuffers (model_t *mod);
int D_FindAliasBuffers (model_t *mod);
int D_GetFreeBufferSet (void);

void Mesh_BuildFrameNormals (meshpolyvert_t *verts, int numverts, const mesh_triangle_t *triangles, int numtris, vertexnormals_t *vnorms);


