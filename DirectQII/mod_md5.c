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
// models.c -- model loading and caching

#include "r_local.h"
#include "mesh.h"


extern model_t *loadmodel;
extern int		modfilelen;

static char md5_token[1024];


/*
==============
MD5_ParseLine

parse a full line out of a text stream
==============
*/
char *MD5_ParseLine (char *data)
{
	int i = 0;

	if (!data) return NULL;		// nothing to parse
	if (!*data) return NULL;	// end of stream

	md5_token[0] = 0;

	while (1)
	{
		if (!*data) break;	// end of stream

		// newline
		if (*data == '\n')
		{
			// skip the line separator
			data++;

			// handle \n\r
			if (*data && *data == '\r') data++;

			break;
		}

		// newline
		if (*data == '\r')
		{
			// skip the line separator
			data++;

			// handle \r\n
			if (*data && *data == '\n') data++;

			break;
		}

		// copy it over
		md5_token[i++] = *data++;
	}

	// skip empty lines
	if (i == 0)
		return MD5_ParseLine (data);

	// NUll-terminate the token
	md5_token[i] = 0;

	// and return the new data pointer
	return data;
}


/*
==============
MD5_Parse

Parse a token out of a string
==============
*/
char *MD5_Parse (char *data)
{
	int             c;
	int             len;

	len = 0;
	md5_token[0] = 0;

	if (!data)
		return NULL;

	// skip whitespace
skipwhite:;
	while ((c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				md5_token[len] = 0;
				return data;
			}
			md5_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
	{
		md5_token[len] = c;
		len++;
		md5_token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		md5_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':')
			break;
	} while (c > 32);

	md5_token[len] = 0;
	return data;
}


/*
==================
MD5_GetFileName

==================
*/
static char *MD5_GetFileName (char *base, char *ext)
{
	static char newname[1024];
	char *find = NULL;

	strcpy (newname, base);

	if ((find = strstr (newname, "/tris.md2")) != NULL)
	{
		strcpy (find, va ("/md5/tris.%s", ext));
	}

	return newname;
}


/*
==================
MD5_BuildFrameSkeleton

Build skeleton for a given frame data.
==================
*/
static void MD5_BuildFrameSkeleton (const joint_info_t *jointInfos, const baseframe_joint_t *baseFrame, const float *animFrameData, md5_joint_t *skelFrame, int num_joints)
{
	int i;

	for (i = 0; i < num_joints; i++)
	{
		const baseframe_joint_t *baseJoint = &baseFrame[i];
		vec3_t animatedPos;
		quat4_t animatedOrient;
		int j = 0;

		memcpy (animatedPos, baseJoint->pos, sizeof (vec3_t));
		memcpy (animatedOrient, baseJoint->orient, sizeof (quat4_t));

		if (jointInfos[i].flags & 1) // Tx
		{
			animatedPos[0] = animFrameData[jointInfos[i].startIndex + j];
			j++;
		}

		if (jointInfos[i].flags & 2) // Ty
		{
			animatedPos[1] = animFrameData[jointInfos[i].startIndex + j];
			j++;
		}

		if (jointInfos[i].flags & 4) // Tz
		{
			animatedPos[2] = animFrameData[jointInfos[i].startIndex + j];
			j++;
		}

		if (jointInfos[i].flags & 8) // Qx
		{
			animatedOrient[0] = animFrameData[jointInfos[i].startIndex + j];
			j++;
		}

		if (jointInfos[i].flags & 16) // Qy
		{
			animatedOrient[1] = animFrameData[jointInfos[i].startIndex + j];
			j++;
		}

		if (jointInfos[i].flags & 32) // Qz
		{
			animatedOrient[2] = animFrameData[jointInfos[i].startIndex + j];
			j++;
		}

		// Compute orient quaternion's w value
		Quat_computeW (animatedOrient);

		// NOTE: we assume that this joint's parent has already been calculated, i.e. joint's ID should never be smaller than its parent ID.
		{
			md5_joint_t *thisJoint = &skelFrame[i];

			int parent = jointInfos[i].parent;
			thisJoint->parent = parent;
			strcpy (thisJoint->name, jointInfos[i].name);

			// Has parent?
			if (thisJoint->parent < 0)
			{
				memcpy (thisJoint->pos, animatedPos, sizeof (vec3_t));
				memcpy (thisJoint->orient, animatedOrient, sizeof (quat4_t));
			}
			else
			{
				md5_joint_t *parentJoint = &skelFrame[parent];
				vec3_t rpos; // Rotated position

				// Add positions
				Quat_rotatePoint (parentJoint->orient, animatedPos, rpos);
				thisJoint->pos[0] = rpos[0] + parentJoint->pos[0];
				thisJoint->pos[1] = rpos[1] + parentJoint->pos[1];
				thisJoint->pos[2] = rpos[2] + parentJoint->pos[2];

				// Concatenate rotations
				Quat_multQuat (parentJoint->orient, animatedOrient, thisJoint->orient);
				Quat_normalize (thisJoint->orient);
			}
		}
	}
}


/*
==================
MD5_ParseAnimFile

==================
*/
static int MD5_ParseAnimFile (char *filename, char *data, md5_anim_t *anim)
{
	joint_info_t *jointInfos = NULL;
	baseframe_joint_t *baseFrame = NULL;
	float *animFrameData = NULL;
	int version;
	int numAnimatedComponents;
	int frame_index;
	int i;

	// Read whole line
	while ((data = MD5_ParseLine (data)) != NULL)
	{
		if (sscanf (md5_token, " MD5Version %d", &version) == 1)
		{
			if (version != 10)
			{
				// Bad version
				ri.Con_Printf (PRINT_DEVELOPER, "Error: bad animation version for \"%s\"\n", filename);
				return 0;
			}
		}
		else if (sscanf (md5_token, " numFrames %d", &anim->num_frames) == 1)
		{
			// Allocate memory for bounding boxes
			if (anim->num_frames > 0)
			{
				// we're going to keep the bboxes so put them in the heap and we can just copy the pointer instead of needing to
				// alloc again and memcpy the data
				anim->bboxes = (md5_bbox_t *) HeapAlloc (loadmodel->hHeap, HEAP_ZERO_MEMORY, sizeof (md5_bbox_t) * anim->num_frames);
			}
		}
		else if (sscanf (md5_token, " numJoints %d", &anim->num_joints) == 1)
		{
			if (anim->num_joints > 0)
			{
				// allocate memory for the joints of each frame
				anim->skeletonframes = (md5_joint_t *) ri.Hunk_Alloc (sizeof (md5_joint_t) * anim->num_joints * anim->num_frames);

				// Allocate temporary memory for building skeleton frames
				jointInfos = (joint_info_t *) ri.Hunk_Alloc (sizeof (joint_info_t) * anim->num_joints);
				baseFrame = (baseframe_joint_t *) ri.Hunk_Alloc (sizeof (baseframe_joint_t) * anim->num_joints);
			}
		}
		else if (sscanf (md5_token, " frameRate %d", &anim->frameRate) == 1)
			; // unused in this code
		else if (sscanf (md5_token, " numAnimatedComponents %d", &numAnimatedComponents) == 1)
		{
			if (numAnimatedComponents > 0)
			{
				// Allocate memory for animation frame data
				animFrameData = (float *) ri.Hunk_Alloc (sizeof (float) * numAnimatedComponents);
			}
		}
		else if (strncmp (md5_token, "hierarchy {", 11) == 0)
		{
			for (i = 0; i < anim->num_joints; i++)
			{
				// Read whole line
				if ((data = MD5_ParseLine (data)) == NULL) return 0;

				// Read joint info
				sscanf (md5_token, " %s %d %d %d", jointInfos[i].name, &jointInfos[i].parent, &jointInfos[i].flags, &jointInfos[i].startIndex);
			}
		}
		else if (strncmp (md5_token, "bounds {", 8) == 0)
		{
			for (i = 0; i < anim->num_frames; i++)
			{
				// Read whole line
				if ((data = MD5_ParseLine (data)) == NULL) return 0;

				// Read bounding box
				sscanf (md5_token, " ( %f %f %f ) ( %f %f %f )", &anim->bboxes[i].mins[0], &anim->bboxes[i].mins[1], &anim->bboxes[i].mins[2], &anim->bboxes[i].maxs[0], &anim->bboxes[i].maxs[1], &anim->bboxes[i].maxs[2]);
			}
		}
		else if (strncmp (md5_token, "baseframe {", 10) == 0)
		{
			for (i = 0; i < anim->num_joints; i++)
			{
				// Read whole line
				if ((data = MD5_ParseLine (data)) == NULL) return 0;

				// Read base frame joint
				if (sscanf (md5_token, " ( %f %f %f ) ( %f %f %f )", &baseFrame[i].pos[0], &baseFrame[i].pos[1], &baseFrame[i].pos[2], &baseFrame[i].orient[0], &baseFrame[i].orient[1], &baseFrame[i].orient[2]) == 6)
				{
					// Compute the w component
					Quat_computeW (baseFrame[i].orient);
				}
			}
		}
		else if (sscanf (md5_token, " frame %d", &frame_index) == 1)
		{
			// Read frame data
			for (i = 0; i < numAnimatedComponents; i++)
			{
				// parse a single token
				if ((data = MD5_Parse (data)) == NULL) return 0;
				animFrameData[i] = atof (md5_token);
			}

			// Build frame skeleton from the collected data
			MD5_BuildFrameSkeleton (jointInfos, baseFrame, animFrameData, &anim->skeletonframes[frame_index * anim->num_joints], anim->num_joints);
		}
	}

	return 1;
}


/*
==================
MD5_LoadAnimFile

Load an MD5 animation from file.
==================
*/
static int MD5_LoadAnimFile (char *filename, md5_anim_t *anim)
{
	char *data = NULL;
	int len;

	if ((len = ri.FS_LoadFile (filename, (void **) &data)) == -1)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "MD5_LoadMeshFile : couldn't open \"%s\"!\n", filename);
		return 0;
	}

	if (MD5_ParseAnimFile (filename, data, anim))
	{
		ri.FS_FreeFile (data);
		return 1;
	}
	else
	{
		ri.FS_FreeFile (data);
		return 0;
	}
}


/*
==================
MD5_ParseMeshFile

==================
*/
static int MD5_ParseMeshFile (char *filename, char *data, md5_model_t *mdl)
{
	int version;
	int curr_mesh = 0;
	int i;

	// Read whole line
	while ((data = MD5_ParseLine (data)) != NULL)
	{
		if (sscanf (md5_token, " MD5Version %d", &version) == 1)
		{
			if (version != 10)
			{
				// Bad version
				ri.Con_Printf (PRINT_DEVELOPER, "MD5_ReadMeshFile : bad model version for \"%s\"\n", filename);
				return 0;
			}
		}
		else if (sscanf (md5_token, " numJoints %d", &mdl->num_joints) == 1)
		{
			if (mdl->num_joints > 0)
			{
				// Allocate memory for base skeleton joints
				mdl->baseSkel = (md5_joint_t *) ri.Hunk_Alloc (mdl->num_joints * sizeof (md5_joint_t));
			}
		}
		else if (sscanf (md5_token, " numMeshes %d", &mdl->num_meshes) == 1)
		{
			if (mdl->num_meshes > 0)
			{
				// Allocate memory for meshes
				mdl->meshes = (md5_mesh_t *) ri.Hunk_Alloc (mdl->num_meshes * sizeof (md5_mesh_t));
			}
		}
		else if (strncmp (md5_token, "joints {", 8) == 0)
		{
			// Read each joint
			for (i = 0; i < mdl->num_joints; i++)
			{
				md5_joint_t *joint = &mdl->baseSkel[i];

				// Read whole line
				if ((data = MD5_ParseLine (data)) == NULL) return 0;

				if (sscanf (md5_token, "%s %d ( %f %f %f ) ( %f %f %f )", joint->name, &joint->parent, &joint->pos[0], &joint->pos[1], &joint->pos[2], &joint->orient[0], &joint->orient[1], &joint->orient[2]) == 8)
				{
					// Compute the w component
					Quat_computeW (joint->orient);
				}
			}
		}
		else if (strncmp (md5_token, "mesh {", 6) == 0)
		{
			md5_mesh_t *mesh = &mdl->meshes[curr_mesh];
			int vert_index = 0;
			int tri_index = 0;
			int weight_index = 0;
			float fdata[4];
			int idata[3];

			while (md5_token[0] != '}')
			{
				// Read whole line
				if ((data = MD5_ParseLine (data)) == NULL) break;

				if (strstr (md5_token, "shader "))
				{
					int quote = 0, j = 0;

					// Copy the shader name whithout the quote marks
					for (i = 0; i < strlen (md5_token) && (quote < 2); i++)
					{
						if (md5_token[i] == '\"')
							quote++;

						if ((quote == 1) && (md5_token[i] != '\"'))
						{
							mesh->shader[j] = md5_token[i];
							j++;
						}
					}
				}
				else if (sscanf (md5_token, " numverts %d", &mesh->num_verts) == 1)
				{
					if (mesh->num_verts > 0)
					{
						// Allocate memory for vertices
						mesh->vertices = (md5_vertex_t *) ri.Hunk_Alloc (sizeof (md5_vertex_t) * mesh->num_verts);
					}
				}
				else if (sscanf (md5_token, " numtris %d", &mesh->num_tris) == 1)
				{
					if (mesh->num_tris > 0)
					{
						// Allocate memory for triangles
						mesh->triangles = (mesh_triangle_t *) ri.Hunk_Alloc (sizeof (mesh_triangle_t) * mesh->num_tris);
					}
				}
				else if (sscanf (md5_token, " numweights %d", &mesh->num_weights) == 1)
				{
					if (mesh->num_weights > 0)
					{
						// Allocate memory for vertex weights
						mesh->weights = (md5_weight_t *) ri.Hunk_Alloc (sizeof (md5_weight_t) * mesh->num_weights);
					}
				}
				else if (sscanf (md5_token, " vert %d ( %f %f ) %d %d", &vert_index, &fdata[0], &fdata[1], &idata[0], &idata[1]) == 5)
				{
					// Copy vertex data
					mesh->vertices[vert_index].st[0] = fdata[0];
					mesh->vertices[vert_index].st[1] = fdata[1];
					mesh->vertices[vert_index].start = idata[0];
					mesh->vertices[vert_index].count = idata[1];
				}
				else if (sscanf (md5_token, " tri %d %d %d %d", &tri_index, &idata[0], &idata[1], &idata[2]) == 4)
				{
					// Copy triangle data
					mesh->triangles[tri_index].index[0] = idata[0];
					mesh->triangles[tri_index].index[1] = idata[1];
					mesh->triangles[tri_index].index[2] = idata[2];
				}
				else if (sscanf (md5_token, " weight %d %d %f ( %f %f %f )", &weight_index, &idata[0], &fdata[3], &fdata[0], &fdata[1], &fdata[2]) == 6)
				{
					// Copy vertex data
					mesh->weights[weight_index].joint = idata[0];
					mesh->weights[weight_index].bias = fdata[3];
					mesh->weights[weight_index].pos[0] = fdata[0];
					mesh->weights[weight_index].pos[1] = fdata[1];
					mesh->weights[weight_index].pos[2] = fdata[2];
				}
			}

			curr_mesh++;
		}
	}

	return 1;
}


/*
==================
MD5_LoadMeshFile

==================
*/
static int MD5_LoadMeshFile (char *filename, md5_model_t *mdl)
{
	char *data = NULL;
	int len;

	if ((len = ri.FS_LoadFile (filename, (void **) &data)) == -1)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "MD5_LoadMeshFile : couldn't open \"%s\"!\n", filename);
		return 0;
	}

	if (MD5_ParseMeshFile (filename, data, mdl))
	{
		ri.FS_FreeFile (data);
		return 1;
	}
	else
	{
		ri.FS_FreeFile (data);
		return 0;
	}
}


/*
==================
MD5_CullboxForFrame

==================
*/
static void MD5_CullboxForFrame (const md5_mesh_t *mesh, const md5_joint_t *skeleton, md5_bbox_t *cullbox)
{
	int i, j;

	// init this cullbox
	cullbox->mins[0] = cullbox->mins[1] = cullbox->mins[2] = 999999;
	cullbox->maxs[0] = cullbox->maxs[1] = cullbox->maxs[2] = -999999;

	// Setup vertices
	for (i = 0; i < mesh->num_verts; i++)
	{
		vec3_t finalVertex = { 0.0f, 0.0f, 0.0f };

		// Calculate final vertex to draw with weights
		for (j = 0; j < mesh->vertices[i].count; j++)
		{
			const md5_weight_t *weight = &mesh->weights[mesh->vertices[i].start + j];
			const md5_joint_t *joint = &skeleton[weight->joint];

			// Calculate transformed vertex for this weight
			vec3_t wv;
			Quat_rotatePoint (joint->orient, weight->pos, wv);

			// The sum of all weight->bias should be 1.0
			finalVertex[0] += (joint->pos[0] + wv[0]) * weight->bias;
			finalVertex[1] += (joint->pos[1] + wv[1]) * weight->bias;
			finalVertex[2] += (joint->pos[2] + wv[2]) * weight->bias;
		}

		// accumulate to the cullbox
		for (j = 0; j < 3; j++)
		{
			if (finalVertex[j] < cullbox->mins[j]) cullbox->mins[j] = finalVertex[j];
			if (finalVertex[j] > cullbox->maxs[j]) cullbox->maxs[j] = finalVertex[j];
		}
	}

	// spread the mins and maxs by 0.5 to ensure we never have zero in any dimension
	for (j = 0; j < 3; j++)
	{
		cullbox->mins[j] -= 0.5f;
		cullbox->maxs[j] += 0.5f;
	}
}


/*
==================
MD5_MakeCullboxes

==================
*/
static void MD5_MakeCullboxes (md5header_t *hdr, md5_mesh_t *mesh, md5_anim_t *anim)
{
	int f, j;

	// init the whole model cullbox
	hdr->fullmins[0] = hdr->fullmins[1] = hdr->fullmins[2] = 999999;
	hdr->fullmaxs[0] = hdr->fullmaxs[1] = hdr->fullmaxs[2] = -999999;

	for (f = 0; f < anim->num_frames; f++)
	{
		// calc the cullbox for this frame
		MD5_CullboxForFrame (mesh, &anim->skeletonframes[f * anim->num_joints], &anim->bboxes[f]);

		// accumulate the frame cullbox to the whole model cullbox
		for (j = 0; j < 3; j++)
		{
			if (anim->bboxes[f].mins[j] < hdr->fullmins[j]) hdr->fullmins[j] = anim->bboxes[f].mins[j];
			if (anim->bboxes[f].maxs[j] > hdr->fullmaxs[j]) hdr->fullmaxs[j] = anim->bboxes[f].maxs[j];
		}
	}
}


/*
==================
MD5_LoadSkins

skin numbers are part of the network protocol so they must match between the base MD2 and it's replacement
==================
*/
void MD5_LoadSkins (model_t *mod, md5header_t *hdr, dmdl_t *pinmodel)
{
	int num_skins = LittleLong (pinmodel->num_skins);
	int ofs_skins = LittleLong (pinmodel->ofs_skins);

	// register all skins
	for (int i = 0; i < num_skins; i++)
	{
		// construct the MD5 replacement skin name from the MD2 source
		char *srcskin = (char *) pinmodel + ofs_skins + i * MAX_SKINNAME;
		char dstskin[MAX_SKINNAME + 10];
		char *name = NULL, *ext = NULL;

		// we're going to be allocating...
		int mark = ri.Hunk_LowMark ();

		// copy it off as the base model data is inviolate
		strcpy (dstskin, srcskin);

		// now chop it at the last / which should separate the path from the filename
		for (int j = strlen (dstskin); j; j--)
		{
			if (dstskin[j] == '/' || dstskin[j] == '\\')
			{
				// chop it off here
				dstskin[j] = 0;

				// take the name from the srcskin so we can safely overwrite dstskin without stomping it
				name = &srcskin[j + 1];

				// append the MD5 path and the filename
				strcat (dstskin, va ("/md5/%s", name));

				// replace the extension with PNG
				if ((ext = strstr (dstskin, ".pcx")) != NULL)
					strcpy (ext, ".png");

				// and done
				break;
			}
		}

		hdr->skinnames[i] = (char *) HeapAlloc (mod->hHeap, HEAP_ZERO_MEMORY, strlen (dstskin) + 1);
		strcpy (hdr->skinnames[i], dstskin);

		mod->skins[i] = GL_FindImage (hdr->skinnames[i], it_skin);

		// ...and free memory
		ri.Hunk_FreeToLowMark (mark);
	}
}


/*
==================
MD5_BuildFrameVertexes

==================
*/
void MD5_BuildFrameVertexes (meshpolyvert_t *dst, const md5_mesh_t *mesh, const md5_joint_t *skeleton)
{
	// Setup vertices
	for (int i = 0; i < mesh->num_verts; i++, dst++)
	{
		// initialize the position vertex
		dst->position[0] = dst->position[1] = dst->position[2] = 0.0f;

		// Calculate final vertex to draw with weights
		for (int j = 0; j < mesh->vertices[i].count; j++)
		{
			const md5_weight_t *weight = &mesh->weights[mesh->vertices[i].start + j];
			const md5_joint_t *joint = &skeleton[weight->joint];

			// Calculate transformed vertex for this weight
			vec3_t wv;
			Quat_rotatePoint (joint->orient, weight->pos, wv);

			// The sum of all weight->bias should be 1.0
			dst->position[0] += (joint->pos[0] + wv[0]) * weight->bias;
			dst->position[1] += (joint->pos[1] + wv[1]) * weight->bias;
			dst->position[2] += (joint->pos[2] + wv[2]) * weight->bias;
		}

		// placeholder
		dst->normal[0] = 1;
		dst->normal[1] = 0;
		dst->normal[2] = 0;
	}
}


/*
==================
MD5_BuildPositionsBuffer

==================
*/
void MD5_BuildPositionsBuffer (bufferset_t *set, const md5_mesh_t *mesh, const md5_anim_t *anim)
{
	// describe the new vertex buffer
	D3D11_BUFFER_DESC vbDesc = {
		mesh->num_verts * sizeof (meshpolyvert_t) * anim->num_frames,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// all memory allocated here is temp and will be thrown away when finished building everything
	int mark = ri.Hunk_LowMark ();

	// allocate a system memory buffer
	meshpolyvert_t *verts = (meshpolyvert_t *) ri.Hunk_Alloc (vbDesc.ByteWidth);

	// set up the SRD for loading
	D3D11_SUBRESOURCE_DATA srd = { verts, 0, 0 };

	// allocate memory for normals
	vertexnormals_t *vnorms = (vertexnormals_t *) ri.Hunk_Alloc (sizeof (vertexnormals_t) * mesh->num_verts);

	// fill it in
	for (int i = 0; i < anim->num_frames; i++)
	{
		// calc the position vertexes for this frame
		MD5_BuildFrameVertexes (verts, mesh, &anim->skeletonframes[i * anim->num_joints]);

		// calc the normals for this frame
		Mesh_BuildFrameNormals (verts, mesh->num_verts, mesh->triangles, mesh->num_tris, vnorms);

		// go to the next frame
		verts += mesh->num_verts;
	}

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->PositionsBuffer);

	// per-frame offset into the buffer
	set->framevertexstride = mesh->num_verts * sizeof (meshpolyvert_t);

	// free all memory used in loading
	ri.Hunk_FreeToLowMark (mark);
}


/*
==================
MD5_BuildTexCoordsBuffer

==================
*/
void MD5_BuildTexCoordsBuffer (bufferset_t *set, const md5_mesh_t *mesh)
{
	// describe the new vertex buffer
	D3D11_BUFFER_DESC vbDesc = {
		mesh->num_verts * sizeof (float) * 2,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	// all memory allocated here is temp and will be thrown away when finished building everything
	int mark = ri.Hunk_LowMark ();

	// allocate a system memory buffer
	float *texcoords = (float *) ri.Hunk_Alloc (vbDesc.ByteWidth);

	// set up the SRD for loading
	D3D11_SUBRESOURCE_DATA srd = { texcoords, 0, 0 };

	// fill it in
	for (int i = 0; i < mesh->num_verts; i++, texcoords += 2)
	{
		texcoords[0] = mesh->vertices[i].st[0];
		texcoords[1] = mesh->vertices[i].st[1];
	}

	// create the new vertex buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, &srd, &set->TexCoordsBuffer);

	// free all memory used in loading
	ri.Hunk_FreeToLowMark (mark);
}


/*
==================
MD5_BuildIndexBuffer

==================
*/
void MD5_BuildIndexBuffer (bufferset_t *set, const md5_mesh_t *mesh)
{
	// describe the new vertex buffer
	D3D11_BUFFER_DESC ibDesc = {
		mesh->num_tris * sizeof (unsigned short) * 3,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	// set up the SRD for loading
	D3D11_SUBRESOURCE_DATA srd = { mesh->triangles, 0, 0 };

	// create the new index buffer
	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &srd, &set->IndexBuffer);

	// copy off the index count for drawing
	set->numindexes = mesh->num_tris * 3;
}


/*
==================
Mod_LoadMD5Model

==================
*/
qboolean Mod_LoadMD5Model (model_t *mod, void *buffer)
{
	// retain a pointer to the original MDL so that we can copy over and validate some data
	dmdl_t *pinmodel = (dmdl_t *) buffer;

	// copy these off for use and validation
	int mdlframes = LittleLong (pinmodel->num_frames);

	// alloc header space
	md5header_t *hdr = (md5header_t *) HeapAlloc (mod->hHeap, HEAP_ZERO_MEMORY, sizeof (md5header_t));

	// load into these but they're not needed post-loading
	md5_model_t mesh = { 0 };
	md5_anim_t anim = { 0 };

	// ...should be already set but just making sure...
	loadmodel = mod;

	// look for a mesh
	if (!MD5_LoadMeshFile (MD5_GetFileName (mod->name, "md5mesh"), &mesh)) goto md5_bad;

	// look for an animation
	if (!MD5_LoadAnimFile (MD5_GetFileName (mod->name, "md5anim"), &anim)) goto md5_bad;

	// validate the frames (these are part of the network protocol so they should match, but sometimes the MD5 has more)
	if (anim.num_frames < mdlframes)
		goto md5_bad; // frames don't match so it can't be used as a drop-in replacement
	else if (anim.num_frames > 65536)
		goto md5_bad; // exceeds protocol maximum

	// the MD5 spec allows for more than 1 mesh but we don't need to support it for Quake21 content
	if (mesh.num_meshes > 1)
		goto md5_bad;

	// the MD5s have additional frames that were not used in the base MD2s; chop them off so that we're not generating buffer data for them.
	anim.num_frames = mdlframes;

	// copy over stuff that's needed post-loading
	hdr->num_frames = anim.num_frames;
	hdr->bboxes = anim.bboxes;

	// load the cullboxes
	// some of the source MD5s were exported with bad cullboxes, so we must regenerate them correctly
	MD5_MakeCullboxes (hdr, mesh.meshes, &anim);

	// see do we already have it
	if ((mod->bufferset = D_FindAliasBuffers (mod)) != -1)
		;	// already loaded
	else if ((mod->bufferset = D_GetFreeBufferSet ()) == -1)
		ri.Sys_Error (ERR_DROP, "Mod_LoadMD5Model : not enough free buffers!");
	else
	{
		// retrieve this buffer set
		bufferset_t *set = R_GetBufferSetForIndex (mod->bufferset);

		// cache the name so that we'll find it next time too
		strcpy (set->Name, mod->name);

		// mark it as active
		set->registration_sequence = r_registration_sequence;

		// now build everything from the model data
		MD5_BuildPositionsBuffer (set, &mesh.meshes[0], &anim);
		MD5_BuildTexCoordsBuffer (set, &mesh.meshes[0]);
		MD5_BuildIndexBuffer (set, &mesh.meshes[0]);
	}

	// load the skins (moved to after the buffer data because it does a Hunk_FreeAll in the texture loader)
	MD5_LoadSkins (mod, hdr, pinmodel);

	// set the type and other data correctly
	mod->type = mod_md5;
	mod->numframes = mdlframes;

	mod->sprheader = NULL;
	mod->md5header = hdr;
	mod->md2header = NULL;

	// free loading memory
	ri.Hunk_FreeAll ();

	// these will be replaced by the correct per-frame bboxes at runtime
	Vector3Set (mod->mins, -32, -32, -32);
	Vector3Set (mod->maxs, 32, 32, 32);

	// and done
	return true;

	// jump-out point for a bad/mismatched replacement
md5_bad:;
	// failed; free all memory and returns false
	ri.Hunk_FreeAll ();

	// this should never happen unless we fail to call Mod_Free properly
	if (mod->hHeap)
	{
		HeapDestroy (mod->hHeap);
		mod->hHeap = NULL;
	}

	// create the memory heap used by this model
	mod->hHeap = HeapCreate (0, 0, 0);

	// didn't load it
	return false;
}


