

struct PS_DRAWSPRITE {
	float4 Position : SV_POSITION;
	float4 Color : COLOUR;
	float2 TexCoord : TEXCOORD;
};

struct PS_PARTICLE {
	float4 Position : SV_POSITION;
	float4 Color : COLOUR;
	float2 TexCoord : TEXCOORD;
};

struct PS_NULL {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
};

#ifdef VERTEXSHADER
PS_DRAWSPRITE SpriteVS (VS_QUADBATCH vs_in)
{
	PS_DRAWSPRITE vs_out;

	vs_out.Position = mul (mvpMatrix, vs_in.Position);
	vs_out.Color = vs_in.Color;
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}

PS_PARTICLE GetParticleVert (VS_QUADBATCH vs_in, float HackUp, float TypeScale)
{
	PS_PARTICLE vs_out;

	// hack a scale up to keep particles from disapearing
	float2 ScaleUp = vs_in.TexCoord * (1.0f + dot (vs_in.Position.xyz - viewOrigin, viewForward) * HackUp);

	// compute new particle origin
	float3 Position = vs_in.Position.xyz + (viewRight * ScaleUp.x + viewUp * ScaleUp.y) * TypeScale;

	// and finally write it out
	vs_out.Position = mul (mvpMatrix, float4 (Position, 1.0f));
	vs_out.Color = vs_in.Color;
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}

PS_PARTICLE ParticleCircleVS (VS_QUADBATCH vs_in)
{
	return GetParticleVert (vs_in, 0.002f, 0.666f);
}

PS_PARTICLE ParticleSquareVS (VS_QUADBATCH vs_in)
{
	return GetParticleVert (vs_in, 0.002f, 0.5f);
}

float4 BeamVS (float4 Position: POSITION) : SV_POSITION
{
	return mul (LocalMatrix, Position);
}

PS_NULL NullVS (float3 Position : POSITION, float3 Normal : NORMAL)
{
	PS_NULL vs_out;

	vs_out.Position = mul (LocalMatrix, float4 (Position * 16.0f, 1));
	vs_out.Normal = Normal;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 SpritePS (PS_DRAWSPRITE ps_in) : SV_TARGET0
{
	// adjust for pre-multiplied alpha
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord)) * ps_in.Color;
	return float4 (diff.rgb * diff.a, diff.a);
}

float4 ParticleCirclePS (PS_PARTICLE ps_in) : SV_TARGET0
{
	// procedurally generate the particle dot for good speed and per-pixel accuracy at any scale
	return GetGamma (float4 (ps_in.Color.rgb, saturate (ps_in.Color.a * (1.0f - dot (ps_in.TexCoord, ps_in.TexCoord)))));
}

float4 ParticleSquarePS (PS_PARTICLE ps_in) : SV_TARGET0
{
	// procedurally generate the particle dot for good speed and per-pixel accuracy at any scale
	return GetGamma (ps_in.Color);
}

float4 BeamPS (float4 Position: SV_POSITION) : SV_TARGET0
{
	return GetGamma (float4 (ShadeColor, AlphaVal));
}

float4 NullPS (PS_NULL ps_in) : SV_TARGET0
{
	float shadedot = dot (normalize (ps_in.Normal), normalize (float3 (1.0f, 1.0f, 1.0f)));
	return GetGamma (float4 (ShadeColor * max (shadedot + 1.0f, (shadedot * 0.2954545f) + 1.0f), AlphaVal));
}
#endif

