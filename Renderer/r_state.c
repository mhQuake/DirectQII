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


#define MAX_VERTEX_STREAMS	16

typedef struct streamdef_s {
	ID3D11Buffer *Buffer;
	UINT Stride;
	UINT Offset;
} streamdef_t;


ID3D11SamplerState *D_CreateSamplerState (D3D11_FILTER Filter, D3D11_TEXTURE_ADDRESS_MODE AddressMode, float MaxLOD, UINT MaxAnisotropy)
{
	ID3D11SamplerState *ss = NULL;
	D3D11_SAMPLER_DESC desc;

	desc.AddressU = AddressMode;
	desc.AddressV = AddressMode;
	desc.AddressW = AddressMode;

	// border colour is always black because that's what our clamp-to-border code elsewhere in the engine expects
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0;

	// nope, not doing this
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	if (MaxAnisotropy > 1)
	{
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.MaxAnisotropy = MaxAnisotropy;
	}
	else
	{
		desc.Filter = Filter;
		desc.MaxAnisotropy = 1;
	}

	desc.MaxLOD = MaxLOD;
	desc.MinLOD = 0;
	desc.MipLODBias = 0;

	d3d_Device->lpVtbl->CreateSamplerState (d3d_Device, &desc, &ss);
	D_CacheObject ((ID3D11DeviceChild *) ss, "ID3D11SamplerState");

	return ss;
}


ID3D11BlendState *D_CreateBlendState (BOOL blendon, D3D11_BLEND src, D3D11_BLEND dst, D3D11_BLEND_OP op)
{
	ID3D11BlendState *bs = NULL;
	D3D11_BLEND_DESC desc;

	desc.AlphaToCoverageEnable = FALSE;
	desc.IndependentBlendEnable = FALSE;
	desc.RenderTarget[0].BlendEnable = blendon;
	desc.RenderTarget[0].SrcBlend = src;
	desc.RenderTarget[0].DestBlend = dst;
	desc.RenderTarget[0].BlendOp = op;
	desc.RenderTarget[0].SrcBlendAlpha = src;
	desc.RenderTarget[0].DestBlendAlpha = dst;
	desc.RenderTarget[0].BlendOpAlpha = op;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	d3d_Device->lpVtbl->CreateBlendState (d3d_Device, &desc, &bs);
	D_CacheObject ((ID3D11DeviceChild *) bs, "ID3D11BlendState");

	return bs;
}


ID3D11DepthStencilState *D_CreateDepthState (BOOL test, BOOL mask, D3D11_COMPARISON_FUNC func)
{
	// to do - add an enum controlling the stencil test; DS_STNONE, DS_STZPASS, DS_STZFAIL, etc...
	ID3D11DepthStencilState *ds = NULL;
	D3D11_DEPTH_STENCIL_DESC desc;

	if (test)
	{
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = mask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	}
	else
	{
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	}

	desc.DepthFunc = func;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

	d3d_Device->lpVtbl->CreateDepthStencilState (d3d_Device, &desc, &ds);
	D_CacheObject ((ID3D11DeviceChild *) ds, "ID3D11DepthStencilState");

	return ds;
}


ID3D11RasterizerState *D_CreateRasterizerState (D3D11_FILL_MODE fill, D3D11_CULL_MODE cull, BOOL clip, BOOL scissor)
{
	ID3D11RasterizerState *rs = NULL;
	D3D11_RASTERIZER_DESC desc;

	desc.FillMode = fill;
	desc.CullMode = cull;
	desc.FrontCounterClockwise = TRUE;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0;
	desc.SlopeScaledDepthBias = 0;
	desc.DepthClipEnable = clip;
	desc.ScissorEnable = scissor;
	desc.MultisampleEnable = FALSE;
	desc.AntialiasedLineEnable = FALSE;

	d3d_Device->lpVtbl->CreateRasterizerState (d3d_Device, &desc, &rs);
	D_CacheObject ((ID3D11DeviceChild *) rs, "ID3D11RasterizerState");

	return rs;
}


ID3D11BlendState *d3d_BSNone = NULL;
ID3D11BlendState *d3d_BSAlphaBlend = NULL;
ID3D11BlendState *d3d_BSAlphaReverse = NULL;
ID3D11BlendState *d3d_BSAlphaPreMult = NULL;
ID3D11BlendState *d3d_BSAdditive = NULL;
ID3D11BlendState *d3d_BSRevSubtract = NULL;

ID3D11DepthStencilState *d3d_DSFullDepth = NULL;
ID3D11DepthStencilState *d3d_DSDepthNoWrite = NULL;
ID3D11DepthStencilState *d3d_DSNoDepth = NULL;
ID3D11DepthStencilState *d3d_DSEqualDepthNoWrite = NULL;

ID3D11RasterizerState *d3d_RSFullCull = NULL;
ID3D11RasterizerState *d3d_RSReverseCull = NULL;
ID3D11RasterizerState *d3d_RSNoCull = NULL;

ID3D11SamplerState *d3d_MainSampler[2] = { NULL, NULL };
ID3D11SamplerState *d3d_LMapSampler[2] = { NULL, NULL };
ID3D11SamplerState *d3d_WarpSampler[2] = { NULL, NULL };
ID3D11SamplerState *d3d_DrawSampler[2] = { NULL, NULL };
ID3D11SamplerState *d3d_CineSampler[2] = { NULL, NULL };


cvar_t *r_crunchypixels = NULL;


void R_SetDefaultState (void)
{
	d3d_BSNone = D_CreateBlendState (FALSE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAlphaBlend = D_CreateBlendState (TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAlphaReverse = D_CreateBlendState (TRUE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAlphaPreMult = D_CreateBlendState (TRUE, D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD);
	d3d_BSAdditive = D_CreateBlendState (TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD);
	d3d_BSRevSubtract = D_CreateBlendState (TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_REV_SUBTRACT);

	d3d_DSFullDepth = D_CreateDepthState (TRUE, TRUE, D3D11_COMPARISON_LESS_EQUAL);
	d3d_DSDepthNoWrite = D_CreateDepthState (TRUE, FALSE, D3D11_COMPARISON_LESS_EQUAL);
	d3d_DSNoDepth = D_CreateDepthState (FALSE, FALSE, D3D11_COMPARISON_ALWAYS);
	d3d_DSEqualDepthNoWrite = D_CreateDepthState (TRUE, FALSE, D3D11_COMPARISON_EQUAL);

	d3d_RSFullCull = D_CreateRasterizerState (D3D11_FILL_SOLID, D3D11_CULL_FRONT, TRUE, FALSE);
	d3d_RSReverseCull = D_CreateRasterizerState (D3D11_FILL_SOLID, D3D11_CULL_BACK, TRUE, FALSE);
	d3d_RSNoCull = D_CreateRasterizerState (D3D11_FILL_SOLID, D3D11_CULL_NONE, TRUE, FALSE);

	// crunchy pixels
	d3d_MainSampler[0] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FLOAT32_MAX, 1);
	d3d_LMapSampler[0] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1);
	d3d_WarpSampler[0] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, 0, 1);
	d3d_DrawSampler[0] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1);
	d3d_CineSampler[0] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_BORDER, 0, 1);

	// smooth pixels
	d3d_MainSampler[1] = D_CreateSamplerState (D3D11_FILTER_ANISOTROPIC, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FLOAT32_MAX, 16);
	d3d_LMapSampler[1] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1);
	d3d_WarpSampler[1] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, 0, 1);
	d3d_DrawSampler[1] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, 0, 1);
	d3d_CineSampler[1] = D_CreateSamplerState (D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_BORDER, 0, 1);
}


ID3D11RasterizerState *R_SelectRasterizerState (int rflags)
{
	if ((rflags & RF_WEAPONMODEL) && r_lefthand->value)
		return d3d_RSReverseCull;
	else return d3d_RSFullCull;
}


void D_BindSamplers (void)
{
	// rebind in case state gets chucked
	if (r_crunchypixels->value)
	{
		ID3D11SamplerState *Samplers[] = {
			d3d_MainSampler[0],
			d3d_LMapSampler[0],
			d3d_WarpSampler[0],
			d3d_DrawSampler[0],
			d3d_CineSampler[0]
		};

		d3d_Context->lpVtbl->PSSetSamplers (d3d_Context, 0, 5, Samplers);
	}
	else
	{
		ID3D11SamplerState *Samplers[] = {
			d3d_MainSampler[1],
			d3d_LMapSampler[1],
			d3d_WarpSampler[1],
			d3d_DrawSampler[1],
			d3d_CineSampler[1]
		};

		d3d_Context->lpVtbl->PSSetSamplers (d3d_Context, 0, 5, Samplers);
	}
}


void D_SetRenderStates (ID3D11BlendState *bs, ID3D11DepthStencilState *ds, ID3D11RasterizerState *rs)
{
	static ID3D11BlendState *oldbs = NULL;
	static ID3D11DepthStencilState *oldds = NULL;
	static ID3D11RasterizerState *oldrs = NULL;

	if (oldbs != bs)
	{
		d3d_Context->lpVtbl->OMSetBlendState (d3d_Context, bs, NULL, 0xffffffff);
		oldbs = bs;
	}

	if (oldds != ds)
	{
		d3d_Context->lpVtbl->OMSetDepthStencilState (d3d_Context, ds, 0);
		oldds = ds;
	}

	if (oldrs != rs)
	{
		d3d_Context->lpVtbl->RSSetState (d3d_Context, rs);
		oldrs = rs;
	}
}


void D_BindVertexBuffer (UINT Slot, ID3D11Buffer *Buffer, UINT Stride, UINT Offset)
{
	static streamdef_t d3d_VertexStreams[MAX_VERTEX_STREAMS];

	if (Slot < 0) return;
	if (Slot >= MAX_VERTEX_STREAMS) return;

	if (d3d_VertexStreams[Slot].Buffer != Buffer ||
		d3d_VertexStreams[Slot].Stride != Stride ||
		d3d_VertexStreams[Slot].Offset != Offset)
	{
		d3d_Context->lpVtbl->IASetVertexBuffers (d3d_Context, Slot, 1, &Buffer, &Stride, &Offset);

		d3d_VertexStreams[Slot].Buffer = Buffer;
		d3d_VertexStreams[Slot].Stride = Stride;
		d3d_VertexStreams[Slot].Offset = Offset;
	}
}


void D_BindIndexBuffer (ID3D11Buffer *Buffer, DXGI_FORMAT Format)
{
	static ID3D11Buffer *OldBuffer = NULL;
	static DXGI_FORMAT OldFormat = DXGI_FORMAT_UNKNOWN;

	if (OldBuffer != Buffer || OldFormat != Format)
	{
		d3d_Context->lpVtbl->IASetIndexBuffer (d3d_Context, Buffer, Format, 0);

		OldBuffer = Buffer;
		OldFormat = Format;
	}
}



