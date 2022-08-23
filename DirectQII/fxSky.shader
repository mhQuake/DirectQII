
struct VS_DRAWSKY {
	float4 Position : POSITION;
};

struct PS_DRAWSKY {
	float4 Position : SV_POSITION;
	float3 TexCoord : TEXCOORD;
};


#ifdef VERTEXSHADER
PS_DRAWSKY SurfDrawSkyVS (VS_DRAWSKY vs_in)
{
	PS_DRAWSKY vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.Position.z = vs_out.Position.w; // fix the skybox to the far clipping plane
	vs_out.TexCoord = mul (SkyMatrix, vs_in.Position).xyz;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 SurfDrawSkyPS (PS_DRAWSKY ps_in) : SV_TARGET0
{
	// reuses the lightmap sampler because it has the same sampler state as is required here
	return GetGamma (sboxTexture.Sample (lmapSampler, ps_in.TexCoord));
}

float4 SkyNoSkyPS (PS_DRAWSKY ps_in) : SV_TARGET0
{
	return float4 (1, 1, 1, 1);
}
#endif

