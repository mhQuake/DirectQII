

struct PS_DRAWTEXTURED {
	float4 Position : SV_POSITION;
	float4 Color : COLOUR;
	float2 TexCoord : TEXCOORD;
};

struct PS_DRAWCOLOURED {
	float4 Position : SV_POSITION;
	float4 Color : COLOUR;
};


#ifdef VERTEXSHADER
PS_DRAWTEXTURED DrawTexturedVS (VS_QUADBATCH vs_in)
{
	PS_DRAWTEXTURED vs_out;

	vs_out.Position = mul (orthoMatrix, vs_in.Position);
	vs_out.Color = vs_in.Color;
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}

PS_DRAWCOLOURED DrawColouredVS (VS_QUADBATCH vs_in)
{
	PS_DRAWCOLOURED vs_out;

	vs_out.Position = mul (orthoMatrix, vs_in.Position);
	vs_out.Color = vs_in.Color;

	return vs_out;
}

float4 DrawPolyblendVS (VS_QUADBATCH vs_in) : SV_POSITION
{
	return vs_in.Position;
}

PS_DRAWTEXTURED DrawFinalizeVS (VS_QUADBATCH vs_in)
{
	PS_DRAWTEXTURED vs_out;

	vs_out.Position = vs_in.Position;
	vs_out.Color = vs_in.Color;
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 DrawTexturedPS (PS_DRAWTEXTURED ps_in) : SV_TARGET0
{
	// adjust for pre-multiplied alpha
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord)) * ps_in.Color;
	return float4 (diff.rgb * diff.a, diff.a);
}

float4 DrawColouredPS (PS_DRAWCOLOURED ps_in) : SV_TARGET0
{
	// adjust for pre-multiplied alpha
	float4 diff = GetGamma (ps_in.Color);
	return float4 (diff.rgb * diff.a, diff.a);
}

float4 DrawPolyblendPS (float4 Position : SV_POSITION) : SV_TARGET0
{
	return GetGamma (vBlend);
}

float4 DrawFinalizePS (PS_DRAWTEXTURED ps_in) : SV_TARGET0
{
	return mainTexture.Sample (mainSampler, ps_in.TexCoord);
}
#endif

