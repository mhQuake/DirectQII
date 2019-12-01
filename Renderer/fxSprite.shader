
struct PS_SPRITE {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};


#ifdef VERTEXSHADER
static const float2 SpriteTexCoords[4] = {float2 (0, 1), float2 (0, 0), float2 (1, 0), float2 (1, 1)};

PS_SPRITE SpriteVS (float2 XYOffset : XYOFFSET, uint vertexId : SV_VertexID)
{
	PS_SPRITE vs_out;

	vs_out.Position = mul (LocalMatrix, float4 ((viewRight * XYOffset.y) + (viewUp * XYOffset.x), 1.0f));
	vs_out.TexCoord = SpriteTexCoords[vertexId];

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 SpritePS (PS_SPRITE ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));
	return float4 (diff.rgb, diff.a * AlphaVal);
}
#endif

