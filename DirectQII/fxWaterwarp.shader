

struct PS_WATERWARP {
	float4 Position : SV_POSITION;
	float2 TexCoord0 : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
};


#ifdef VERTEXSHADER
PS_WATERWARP WaterWarpVS (uint vertexId : SV_VertexID)
{
	PS_WATERWARP vs_out;

	vs_out.Position = float4 ((float) (vertexId / 2) * 4.0f - 1.0f, (float) (vertexId % 2) * 4.0f - 1.0f, 0, 1);
	vs_out.TexCoord0 = float2 ((float) (vertexId / 2) * 2.0f, 1.0f - (float) (vertexId % 2) * 2.0f);
	vs_out.TexCoord1 = vs_out.TexCoord0 * float2 (RefdefW / RefdefH, 1.0f);

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 WaterWarpPS (PS_WATERWARP ps_in) : SV_TARGET0
{
	// run the underwater warp, merging it with the polyblend for added perf
	// scroll the warp in opposite dirs so that it doesn't cycle so obviously
	// offsetting the timescale slightly too to help with the effect

	// read the noise image
	float2 warp = float2 (
		// fixme - move warptime to VS
		warpTexture.Sample (warpSampler, ps_in.TexCoord1 + WarpTime.x).r,
		warpTexture.Sample (warpSampler, ps_in.TexCoord1 - WarpTime.y).g
	) * (1.0f / 64.0f);

	// don't apply gamma because it's just reading back an already-corrected scene
	return lerp (mainTexture.Sample (lmapSampler, ps_in.TexCoord0 + warp), GetGamma (vBlend), vBlend.a);
}
#endif

