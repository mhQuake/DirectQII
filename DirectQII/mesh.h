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
	ID3D11Buffer *PositionsBuffer;
	ID3D11Buffer *TexCoordsBuffer;
	ID3D11Buffer *IndexBuffer;

	char Name[256];
	int registration_sequence;

	// for drawing from
	int framevertexstride;
	int numindexes;
} bufferset_t;

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


bufferset_t *R_GetBufferSetForIndex (int index);
void R_RegisterBufferSet (model_t *mod);
int R_GetBufferSetForModel (model_t *mod);
int R_GetFreeBufferSet (void);

void Mesh_BuildFrameNormals (meshpolyvert_t *verts, int numverts, const mesh_triangle_t *triangles, int numtris, vertexnormals_t *vnorms);


/*
==============================================================================

		MD5 MODELS

==============================================================================
*/

// Joint
typedef struct md5_joint_s {
	char name[64];
	int parent;

	vec3_t pos;
	quat4_t orient;
} md5_joint_t;

// Vertex
typedef struct md5_vertex_s {
	vec2_t st;

	int start; // start weight
	int count; // weight count
} md5_vertex_t;

// Weight
typedef struct md5_weight_s {
	int joint;
	float bias;

	vec3_t pos;
} md5_weight_t;

// Bounding box
typedef struct md5_bbox_s {
	vec3_t mins;
	vec3_t maxs;
} md5_bbox_t;

// MD5 mesh
typedef struct md5_mesh_s {
	md5_vertex_t *vertices;
	struct mesh_triangle_s *triangles;
	md5_weight_t *weights;

	int num_verts;
	int num_tris;
	int num_weights;

	char shader[256];
} md5_mesh_t;

// MD5 model structure
typedef struct md5_model_s {
	md5_joint_t *baseSkel;
	md5_mesh_t *meshes;

	int num_joints;
	int num_meshes;
} md5_model_t;

// Animation data
typedef struct md5_anim_s {
	int num_frames;
	int num_joints;
	int frameRate;

	// this is the actual animated skeleton
	md5_joint_t *skeleton;

	// and these are the skeleton frames that will be animated
	md5_joint_t *skeletonframes;
	md5_bbox_t *bboxes;
} md5_anim_t;


// Joint info
typedef struct joint_info_s {
	char name[64];
	int parent;
	int flags;
	int startIndex;
} joint_info_t;

// Base frame joint
typedef struct baseframe_joint_s {
	vec3_t pos;
	quat4_t orient;
} baseframe_joint_t;


// !!!!! if this struct is changed, the value of MD5_BINARY_VERSION should be bumped !!!!!
typedef struct md5header_s {
	// stuff from the mesh and anim that's needed after loading
	int num_frames;
	md5_bbox_t *bboxes;

	// full cullbox for use by efrags and other functions where we need a box but don't know the frames yet
	float fullmins[3];
	float fullmaxs[3];

	// vertex buffers
	int	bufferset;

	char *skinnames[MAX_MD2SKINS];
	int numskins;
} md5header_t;


