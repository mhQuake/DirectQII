

struct PS_WATERWARP {
	float4 Position : SV_POSITION;
	float2 TexCoord0 : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
};


#ifdef VERTEXSHADER
static const float2 FullScreenQuadTexCoords[4] = {
	float2 (0, 1),
	float2 (1, 1),
	float2 (1, 0),
	float2 (0, 0)
};

PS_WATERWARP WaterWarpVS (VS_QUADBATCH vs_in, uint vertexId : SV_VertexID)
{
	PS_WATERWARP vs_out;

	vs_out.Position = vs_in.Position;
	vs_out.TexCoord0 = FullScreenQuadTexCoords[vertexId];
	vs_out.TexCoord1 = vs_in.TexCoord;

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

