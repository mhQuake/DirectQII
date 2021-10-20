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

static ID3D11Buffer *d3d_ParticleVertexes = NULL;

static const int MAX_GPU_PARTICLES = (MAX_PARTICLES * 10);
static int r_FirstParticle = 0;

void R_InitParticles (void)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		VDECL ("ORIGIN", 0, DXGI_FORMAT_R32G32B32_FLOAT, 6, 0),
		VDECL ("VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 6, 0),
		VDECL ("ACCELERATION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 6, 0),
		VDECL ("TIME", 0, DXGI_FORMAT_R32_FLOAT, 6, 0),
		VDECL ("COLOR", 0, DXGI_FORMAT_R32_SINT, 6, 0),
		VDECL ("ALPHA", 0, DXGI_FORMAT_R32_FLOAT, 6, 0),
		VDECL ("SIZE", 0, DXGI_FORMAT_R32_SINT, 6, 0)
	};

	D3D11_BUFFER_DESC vbDesc = {
		sizeof (particle_t) * MAX_GPU_PARTICLES,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_VERTEX_BUFFER,
		D3D11_CPU_ACCESS_WRITE,
		0,
		0
	};

	d3d_Device->lpVtbl->CreateBuffer (d3d_Device, &vbDesc, NULL, &d3d_ParticleVertexes);
	D_CacheObject ((ID3D11DeviceChild *) d3d_ParticleVertexes, "d3d_ParticleVertexes");

	// creating a square shader even though we don't currently use it
	d3d_ParticleCircleShader = D_CreateShaderBundle (IDR_PARTSHADER, "ParticleVS", "ParticleCircleGS", "ParticleCirclePS", DEFINE_LAYOUT (layout));
	d3d_ParticleSquareShader = D_CreateShaderBundle (IDR_PARTSHADER, "ParticleVS", "ParticleSquareGS", "ParticleSquarePS", DEFINE_LAYOUT (layout));
}


void R_DrawParticles (void)
{
	D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
	D3D11_MAPPED_SUBRESOURCE msr;

	if (!r_newrefdef.num_particles)
		return;

	if (r_FirstParticle + r_newrefdef.num_particles >= MAX_GPU_PARTICLES)
	{
		r_FirstParticle = 0;
		mode = D3D11_MAP_WRITE_DISCARD;
	}

	if (SUCCEEDED (d3d_Context->lpVtbl->Map (d3d_Context, (ID3D11Resource *) d3d_ParticleVertexes, 0, mode, 0, &msr)))
	{
		// copy over the particles and unmap the buffer
		memcpy ((particle_t *) msr.pData + r_FirstParticle, r_newrefdef.particles, r_newrefdef.num_particles * sizeof (particle_t));
		d3d_Context->lpVtbl->Unmap (d3d_Context, (ID3D11Resource *) d3d_ParticleVertexes, 0);

		// go to points for the geometry shader
		// (we could alternatively attach an index buffer with indices 0|0|0|1|1|1|2|2|2 etc, and do triangle-to-quad expansion, which would be hellishly cute)
		d3d_Context->lpVtbl->IASetPrimitiveTopology (d3d_Context, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		// square particles can potentially expose a faster path by not using alpha blending
		// but we might wish to add particle fade at some time so we can't do it (note: all particles in Q2 have fade)
		D_SetRenderStates (d3d_BSAlphaPreMult, d3d_DSDepthNoWrite, d3d_RSFullCull);

		if (r_crunchypixels->value)
			D_BindShaderBundle (d3d_ParticleSquareShader);
		else D_BindShaderBundle (d3d_ParticleCircleShader);

		D_BindVertexBuffer (6, d3d_ParticleVertexes, sizeof (particle_t), 0);

		// and draw it
		d3d_Context->lpVtbl->Draw (d3d_Context, r_newrefdef.num_particles, r_FirstParticle);

		// back to triangles
		d3d_Context->lpVtbl->IASetPrimitiveTopology (d3d_Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// and go to the next particle batch
		r_FirstParticle += r_newrefdef.num_particles;
	}

	r_newrefdef.num_particles = 0;
}


