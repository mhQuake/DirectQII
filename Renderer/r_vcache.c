/*
Copyright (C) 2008 Martin Storsjo

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

/*
=========================================
BASE: https://github.com/vivkin/forsyth

MODIFICATIONS
- brace style to Allman
- tabs instead of spaces
- .c instead of .h
- remove some of the generalized stuff
- use our custom allocators instead of malloc and free
- remove include guards and .cpp stuff
- some renaming
- remove C99isms
- added "do not optimize" case for when model fits entirely in cache
=========================================
*/

#include "r_local.h"

// Set these to adjust the performance and result quality
#define VERTEX_CACHE_SIZE 24
#define CACHE_FUNCTION_LENGTH 32

// The size of these data types affect the memory usage
#define SCORE_SCALING 7281
#define MAX_ADJACENCY 255

// The size of the precalculated tables
#define CACHE_SCORE_TABLE_SIZE 32
#define VALENCE_SCORE_TABLE_SIZE 32

#if CACHE_SCORE_TABLE_SIZE < VERTEX_CACHE_SIZE
#error Vertex score table too small
#endif

// Precalculated tables
static unsigned short CachePositionScore[CACHE_SCORE_TABLE_SIZE];
static unsigned short ValenceScore[VALENCE_SCORE_TABLE_SIZE];

// Score function constants
#define CACHE_DECAY_POWER 1.5
#define LAST_TRI_SCORE 0.75
#define VALENCE_BOOST_SCALE 2.0
#define VALENCE_BOOST_POWER 0.5

// Precalculate the tables
void VCache_Init (void)
{
	int i;

	for (i = 0; i < CACHE_SCORE_TABLE_SIZE; i++)
	{
		float score = 0;

		if (i < 3)
		{
			// This vertex was used in the last triangle,
			// so it has a fixed score, which ever of the three
			// it's in. Otherwise, you can get very different
			// answers depending on whether you add
			// the triangle 1,2,3 or 3,1,2 - which is silly
			score = LAST_TRI_SCORE;
		}
		else
		{
			// Points for being high in the cache.
			const float scaler = 1.0f / (CACHE_FUNCTION_LENGTH - 3);
			score = 1.0f - (i - 3) * scaler;
			score = powf (score, CACHE_DECAY_POWER);
		}

		CachePositionScore[i] = (unsigned short) (SCORE_SCALING * score);
	}

	for (i = 1; i < VALENCE_SCORE_TABLE_SIZE; i++)
	{
		// Bonus points for having a low number of tris still to
		// use the vert, so we get rid of lone verts quickly
		float valenceBoost = powf (i, -VALENCE_BOOST_POWER);
		float score = VALENCE_BOOST_SCALE * valenceBoost;
		ValenceScore[i] = (unsigned short) (SCORE_SCALING * score);
	}
}


// Calculate the score for a vertex
static unsigned short FindVertexScore (int numActiveTris, int cachePosition)
{
	unsigned short score = 0;

	if (numActiveTris == 0)
	{
		// No triangles need this vertex!
		return 0;
	}

	if (cachePosition < 0)
		; // Vertex is not in LRU cache - no score
	else score = CachePositionScore[cachePosition];

	if (numActiveTris < VALENCE_SCORE_TABLE_SIZE)
		score += ValenceScore[numActiveTris];

	return score;
}


#define VC_OK				0
#define VS_ONETRIANGLE		1
#define VC_SMALLMODEL		2
#define VC_TOOCOMPLEX		3
#define VC_BADCACHETAG		4

// The main reordering function
static int VCache_ActuallyReorderIndices (unsigned short *outIndices, const unsigned short *indices, int nTriangles, int nVertices)
{
	int i, j, sum, bestTriangle, bestScore, outPos, scanPos;
	int cache[VERTEX_CACHE_SIZE + 3];
	byte *numActiveTris = (byte *) ri.Load_AllocMemory (sizeof (byte) * nVertices);

	int *offsets;
	unsigned short *lastScore;
	char *cacheTag;

	byte *triangleAdded;
	unsigned short *triangleScore;
	int *triangleIndices;

	int *outTriangles;

	// mesh with a single triangle, no optimization needed
	if (nTriangles == 1) return VS_ONETRIANGLE;

	// entire mesh fits in the cache, no optimization needed
	if (nVertices < VERTEX_CACHE_SIZE) return VC_SMALLMODEL;

	// First scan over the vertex data, count the total number of occurrances of each vertex
	for (i = 0; i < 3 * nTriangles; i++)
	{
		// Unsupported mesh, vertex shared by too many triangles
		if (numActiveTris[indices[i]] == MAX_ADJACENCY)
			return VC_TOOCOMPLEX;

		numActiveTris[indices[i]]++;
	}

	// Allocate the rest of the arrays
	offsets = (int *) ri.Load_AllocMemory (sizeof (int) * nVertices);
	lastScore = (unsigned short *) ri.Load_AllocMemory (sizeof (unsigned short) * nVertices);
	cacheTag = (char *) ri.Load_AllocMemory (sizeof (char) * nVertices);

	//triangleAdded = (byte *) ri.Load_AllocMemory ((nTriangles + 7) / 8);
	triangleAdded = (byte *) ri.Load_AllocMemory (nTriangles);
	triangleScore = (unsigned short *) ri.Load_AllocMemory (sizeof (unsigned short) * nTriangles);
	triangleIndices = (int *) ri.Load_AllocMemory (sizeof (int) * 3 * nTriangles);

	// Count the triangle array offset for each vertex,
	// initialize the rest of the data.
	sum = 0;

	for (i = 0; i < nVertices; i++)
	{
		offsets[i] = sum;
		sum += numActiveTris[i];
		numActiveTris[i] = 0;
		cacheTag[i] = -1;
	}

	// Fill the vertex data structures with indices to the triangles using each vertex
	for (i = 0; i < nTriangles; i++)
	{
		for (j = 0; j < 3; j++)
		{
			int v = indices[3 * i + j];
			triangleIndices[offsets[v] + numActiveTris[v]] = i;
			numActiveTris[v]++;
		}
	}

	// Initialize the score for all vertices
	for (i = 0; i < nVertices; i++)
	{
		lastScore[i] = FindVertexScore (numActiveTris[i], cacheTag[i]);

		for (j = 0; j < numActiveTris[i]; j++)
			triangleScore[triangleIndices[offsets[i] + j]] += lastScore[i];
	}

	// Find the best triangle
	bestTriangle = -1;
	bestScore = -1;

	for (i = 0; i < nTriangles; i++)
	{
		if (triangleScore[i] > bestScore)
		{
			bestScore = triangleScore[i];
			bestTriangle = i;
		}
	}

	// Allocate the output array
	outTriangles = (int *) ri.Load_AllocMemory (sizeof (int) * nTriangles);
	outPos = 0;

	// Initialize the cache
	for (i = 0; i < VERTEX_CACHE_SIZE + 3; i++)
		cache[i] = -1;

	scanPos = 0;

	// Output the currently best triangle, as long as there are triangles left to output
	while (bestTriangle >= 0)
	{
		// Mark the triangle as added
		//triangleAdded[bestTriangle >> 3] |= 1 << (bestTriangle & 7);
		triangleAdded[bestTriangle] = 1;

		// Output this triangle
		outTriangles[outPos++] = bestTriangle;

		for (i = 0; i < 3; i++)
		{
			// Update this vertex
			int v = indices[3 * bestTriangle + i];

			// Check the current cache position, if it is in the cache
			int endpos = cacheTag[v];

			if (endpos < 0)
				endpos = VERTEX_CACHE_SIZE + i;

			// this happens on some of the Q2 models and if it happens we need to bail with an unoptimized mesh
			if (endpos >= VERTEX_CACHE_SIZE + 3)
				return VC_BADCACHETAG;

			// Move all cache entries from the previous position in the cache to the new target position (i) one step backwards
			for (j = endpos; j > i; j--)
			{
				cache[j] = cache[j - 1];

				// If this cache slot contains a real vertex, update its cache tag
				if (cache[j] >= 0)
				{
					cacheTag[cache[j]]++;

					// this happens on some of the Q2 models and if it happens we need to bail with an unoptimized mesh
					if (cacheTag[cache[j]] >= VERTEX_CACHE_SIZE + 3)
						return VC_BADCACHETAG;
				}
			}

			// Insert the current vertex into its new target slot
			cache[i] = v;
			cacheTag[v] = i;

			// Find the current triangle in the list of active triangles and remove it
			// (moving the last triangle in the list to the slot of this triangle).
			for (j = 0; j < numActiveTris[v]; j++)
			{
				if (triangleIndices[offsets[v] + j] == bestTriangle)
				{
					triangleIndices[offsets[v] + j] = triangleIndices[offsets[v] + numActiveTris[v] - 1];
					break;
				}
			}

			// Shorten the list
			numActiveTris[v]--;
		}

		// Update the scores of all triangles in the cache
		for (i = 0; i < VERTEX_CACHE_SIZE + 3; i++)
		{
			int v = cache[i];
			unsigned short newScore;
			unsigned short diff;

			if (v < 0)
				break;

			// This vertex has been pushed outside of the actual cache
			if (i >= VERTEX_CACHE_SIZE)
			{
				cacheTag[v] = -1;
				cache[i] = -1;
			}

			newScore = FindVertexScore (numActiveTris[v], cacheTag[v]);
			diff = newScore - lastScore[v];

			for (j = 0; j < numActiveTris[v]; j++)
				triangleScore[triangleIndices[offsets[v] + j]] += diff;

			lastScore[v] = newScore;
		}

		// Find the best triangle referenced by vertices in the cache
		bestTriangle = -1;
		bestScore = -1;

		for (i = 0; i < VERTEX_CACHE_SIZE; i++)
		{
			int v = cache[i];

			if (v < 0)
				break;

			for (j = 0; j < numActiveTris[v]; j++)
			{
				int t = triangleIndices[offsets[v] + j];

				if (triangleScore[t] > bestScore)
				{
					bestTriangle = t;
					bestScore = triangleScore[t];
				}
			}
		}

		// If no active triangle was found at all, continue scanning the whole list of triangles
		if (bestTriangle < 0)
		{
			for (; scanPos < nTriangles; scanPos++)
			{
				//if (!(triangleAdded[scanPos >> 3] & (1 << (scanPos & 7))))
				if (!triangleAdded[scanPos])
				{
					bestTriangle = scanPos;
					break;
				}
			}
		}
	}

	// Convert the triangle index array into a full triangle list
	outPos = 0;

	for (i = 0; i < nTriangles; i++)
	{
		int t = outTriangles[i];

		for (j = 0; j < 3; j++)
		{
			int v = indices[3 * t + j];
			outIndices[outPos++] = v;
		}
	}

	// done!
	return VC_OK;
}


void VCache_ReorderIndices (char *name, unsigned short *outIndices, const unsigned short *indices, int nTriangles, int nVertices)
{
	int stat = VC_OK;

	if ((stat = VCache_ActuallyReorderIndices (outIndices, indices, nTriangles, nVertices)) != VC_OK)
	{
#if 0
		ri.Con_Printf (PRINT_ALL, "%s will not be optimized ... ", name);

		switch (stat)
		{
		case VS_ONETRIANGLE: ri.Con_Printf (PRINT_ALL, "One triangle\n"); break;
		case VC_SMALLMODEL: ri.Con_Printf (PRINT_ALL, "Few vertices\n"); break;
		case VC_TOOCOMPLEX: ri.Con_Printf (PRINT_ALL, "Too complex\n"); break;
		case VC_BADCACHETAG: ri.Con_Printf (PRINT_ALL, "Bad cache tag\n"); break;
		default: ri.Con_Printf (PRINT_ALL, "Other crap\n");
		}
#endif

		// if we didn't reorder for whatever reason, just copy input over to output
		memcpy (outIndices, indices, sizeof (unsigned short) * nTriangles * 3);
	}
}

