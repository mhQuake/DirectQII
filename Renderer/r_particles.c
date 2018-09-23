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


static int d3d_ParticleCircleShader;
static int d3d_ParticleSquareShader;
static int d3d_ParticleShader = 0;

void R_InitParticles (void)
{
	d3d_ParticleCircleShader = D_CreateShaderBundleForQuadBatch (IDR_MISCSHADER, "ParticleCircleVS", "ParticleCirclePS", batch_standard);
	d3d_ParticleSquareShader = D_CreateShaderBundleForQuadBatch (IDR_MISCSHADER, "ParticleSquareVS", "ParticleSquarePS", batch_standard);
	d3d_ParticleShader = d3d_ParticleCircleShader;
}

void D_SetParticleMode (D3D11_FILTER mode)
{
	switch (mode)
	{
		// all of the modes for which MAG is POINT choose the square particle mode
		// not all of these are currently exposed through gl_texturemode, but we check them anyway in case we ever do so
	case D3D11_FILTER_MIN_MAG_MIP_POINT:
	case D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR:
	case D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT:
	case D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		d3d_ParticleShader = d3d_ParticleSquareShader;
		break;

	default:
		// any other mode, including ANISOTROPIC, chooses the circle particle mode
		d3d_ParticleShader = d3d_ParticleCircleShader;
		break;
	}
}


/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if (r_newrefdef.num_particles)
	{
		int i;

		// square particles can potentially expose a faster path by not using alpha blending
		// but we might wish to add particle fade at some time so we can't do it
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSFullCull);
		D_BindShaderBundle (d3d_ParticleShader);

		for (i = 0; i < r_newrefdef.num_particles; i++)
		{
			particle_t *p = &r_newrefdef.particles[i];

			D_CheckQuadBatch ();

			D_QuadVertexPosition3fvColorAlphaTexCoord2f (p->origin, d_8to24table[p->color & 255], p->alphaval, -1, -1);
			D_QuadVertexPosition3fvColorAlphaTexCoord2f (p->origin, d_8to24table[p->color & 255], p->alphaval, -1,  1);
			D_QuadVertexPosition3fvColorAlphaTexCoord2f (p->origin, d_8to24table[p->color & 255], p->alphaval,  1,  1);
			D_QuadVertexPosition3fvColorAlphaTexCoord2f (p->origin, d_8to24table[p->color & 255], p->alphaval,  1, -1);
		}

		D_EndQuadBatch ();
		r_newrefdef.num_particles = 0;
	}
}

