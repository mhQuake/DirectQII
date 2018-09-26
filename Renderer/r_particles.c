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

static ID3D11Buffer *d3d_ParticlePerVertex = NULL;
static ID3D11Buffer *d3d_ParticlePerInstance = NULL;
static ID3D11Buffer *d3d_ParticleIndexes = NULL;

typedef struct partpolyvert_s
{
	float origin[3];
	union {
		DWORD color;
		byte rgba[4];
	};
} partpolyvert_t;

static const int MAX_GPU_PARTICLES = (MAX_PARTICLES * 10);
static int r_FirstParticle = 0;

void R_InitParticles (void)
{
	unsigned short indexes[6] = {0, 1, 2, 0, 2, 3};
	float offsets[4][2] = {{-1, -1}, {-1,  1}, {1,  1}, {1, -1}};

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("ORIGIN", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 1),
		VDECL ("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1),
		VDECL ("OFFSETS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0)
	};

	D3D11_BUFFER_DESC ibDesc = {
		sizeof (indexes),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_INDEX_BUFFER,
		0,
		0,
		0
	};

	D3D11_BUFFER_DESC vbPerVertDesc = {
		sizeof (offsets),
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_VERTEX_BUFFER,
		0,
		0,
		0
	};

	D3D11_BUFFER_DESC vbPerInstDesc = {
		sizeof (partpolyvert_t) * MAX_GPU_PARTICLES,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_VERTEX_BUFFER,
		D3D11_CPU_ACCESS_WRITE,
		0,
		0
	};

	D3D11_SUBRESOURCE_DATA ibsrd = {indexes, 0, 0};
	D3D11_SUBRESOURCE_DATA vbsrd = {offsets, 0, 0};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &ibDesc, &ibsrd, &d3d_ParticleIndexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_ParticleIndexes, "d3d_ParticleIndexes");

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbPerVertDesc, &vbsrd, &d3d_ParticlePerVertex);
	D_CacheObject ((ID3D11DeviceChild *) d3d_ParticlePerVertex, "d3d_ParticlePerVertex");

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbPerInstDesc, NULL, &d3d_ParticlePerInstance);
	D_CacheObject ((ID3D11DeviceChild *) d3d_ParticlePerInstance, "d3d_ParticlePerInstance");

	d3d_ParticleCircleShader = D_CreateShaderBundle (IDR_MISCSHADER, "ParticleInstancedCircleVS", NULL, "ParticleCirclePS", DEFINE_LAYOUT (layout));
	d3d_ParticleSquareShader = D_CreateShaderBundle (IDR_MISCSHADER, "ParticleInstancedSquareVS", NULL, "ParticleSquarePS", DEFINE_LAYOUT (layout));
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
		D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
		D3D11_MAPPED_SUBRESOURCE msr;

		// square particles can potentially expose a faster path by not using alpha blending
		// but we might wish to add particle fade at some time so we can't do it
		D_SetRenderStates (d3d_BSAlphaBlend, d3d_DSDepthNoWrite, d3d_RSFullCull);
		D_BindShaderBundle (d3d_ParticleShader);

		D_BindVertexBuffer (0, d3d_ParticlePerInstance, sizeof (partpolyvert_t), 0);
		D_BindVertexBuffer (1, d3d_ParticlePerVertex, sizeof (float) * 2, 0);
		D_BindIndexBuffer (d3d_ParticleIndexes, DXGI_FORMAT_R16_UINT);

		if (r_FirstParticle + r_newrefdef.num_particles >= MAX_GPU_PARTICLES)
		{
			r_FirstParticle = 0;
			mode = D3D11_MAP_WRITE_DISCARD;
		}

		if (SUCCEEDED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) d3d_ParticlePerInstance, 0, mode, 0, &msr)))
		{
			int i;
			partpolyvert_t *ppv = (partpolyvert_t *) msr.pData + r_FirstParticle;

			for (i = 0; i < r_newrefdef.num_particles; i++, ppv++)
			{
				particle_t *p = &r_newrefdef.particles[i];

				ppv->origin[0] = p->origin[0];
				ppv->origin[1] = p->origin[1];
				ppv->origin[2] = p->origin[2];
				ppv->color = d_8to24table[p->color & 255];
				ppv->rgba[3] = p->alphaval;
			}

			d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) d3d_ParticlePerInstance, 0);
			d3d_Context->lpVtbl->DrawIndexedInstanced (d3d_Context, 6, r_newrefdef.num_particles, 0, 0, r_FirstParticle);

			r_FirstParticle += r_newrefdef.num_particles;
		}

		r_newrefdef.num_particles = 0;
	}
}

