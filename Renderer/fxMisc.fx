
struct VS_PARTICLE {
	float3 Origin : ORIGIN;
	float3 Velocity : VELOCITY;
	float3 Acceleration : ACCELERATION;
	float Time : TIME;
	int Color : COLOR;
	float Alpha : ALPHA;
};

struct GS_PARTICLE {
	float3 Origin : ORIGIN;
	float4 Color : COLOR;
};

struct PS_SPRITE {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

struct PS_PARTICLE {
	float4 Position : SV_POSITION;
	float4 Color : COLOUR;
	float2 Offsets : OFFSETS;
};

struct PS_NULL {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
};

#ifdef VERTEXSHADER
float SpriteVS (uint vertexId : SV_VertexID) : BULLSHIT
{
	// not sure if a NULL VS is allowed but that's what we really actually want here, so we just do it this way instead
	return 1.0f;
}

GS_PARTICLE ParticleVS (VS_PARTICLE vs_in)
{
	GS_PARTICLE vs_out;

	// move the particle in a framerate-independent manner
	vs_out.Origin = vs_in.Origin + (vs_in.Velocity + vs_in.Acceleration * vs_in.Time) * vs_in.Time;

	// copy over colour
	vs_out.Color = float4 (QuakePalette.Load (vs_in.Color).rgb, vs_in.Alpha);

	return vs_out;
}

float4 BeamVS (float4 Position: POSITION) : SV_POSITION
{
	return mul (LocalMatrix, Position);
}

static const uint NULLIndexes[24] = {1, 4, 2, 1, 2, 5, 1, 5, 3, 1, 3, 4, 0, 4, 3, 0, 3, 5, 0, 5, 2, 0, 2, 4};

static const float3 NULLPositions[6] = {
	float3 (0.0f, 0.0f, 1.0f),
	float3 (0.0f, 0.0f, -1.0f),
	float3 (0.0f, 1.0f, 0.0f),
	float3 (0.0f, -1.0f, 0.0f),
	float3 (1.0f, 0.0f, 0.0f),
	float3 (-1.0f, 0.0f, 0.0f)
};

float3 NullVS (uint vertexId : SV_VertexID) : POSITION
{
	return NULLPositions[NULLIndexes[vertexId]];
}
#endif


#ifdef GEOMETRYSHADER
PS_NULL GetNullVert (float3 Position, float3 Normal)
{
	PS_NULL gs_out;

	gs_out.Position = mul (LocalMatrix, float4 (Position * 16.0f, 1));
	gs_out.Normal = Normal;

	return gs_out;
}

[maxvertexcount (3)]
void NullGS (triangle float3 gs_in[3] : POSITION, inout TriangleStream<PS_NULL> gs_out)
{
	// this is the same normal calculation as QBSP does
	float3 Normal = normalize (cross (gs_in[0] - gs_in[1], gs_in[2] - gs_in[1]));

	gs_out.Append (GetNullVert (gs_in[0], Normal));
	gs_out.Append (GetNullVert (gs_in[1], Normal));
	gs_out.Append (GetNullVert (gs_in[2], Normal));
}

PS_PARTICLE GetParticleVert (point GS_PARTICLE gs_in, float2 Offsets, float ScaleUp)
{
	PS_PARTICLE gs_out;

	// compute new particle origin
	float3 Position = gs_in.Origin + (viewRight * Offsets.x + viewUp * Offsets.y) * ScaleUp;

	// and write it out
	gs_out.Position = mul (mvpMatrix, float4 (Position, 1.0f));
	gs_out.Color = gs_in.Color;
	gs_out.Offsets = Offsets;

	return gs_out;
}

void ParticleCommonGS (point GS_PARTICLE gs_in, inout TriangleStream<PS_PARTICLE> gs_out, float TypeScale, float HackUp)
{
	// hack a scale up to keep particles from disapearing
	float ScaleUp = (1.0f + dot (gs_in.Origin - viewOrigin, viewForward) * HackUp) * TypeScale;

	gs_out.Append (GetParticleVert (gs_in, float2 (-1, -1), ScaleUp));
	gs_out.Append (GetParticleVert (gs_in, float2 (-1,  1), ScaleUp));
	gs_out.Append (GetParticleVert (gs_in, float2 ( 1, -1), ScaleUp));
	gs_out.Append (GetParticleVert (gs_in, float2 ( 1,  1), ScaleUp));
}

[maxvertexcount (4)]
void ParticleCircleGS (point GS_PARTICLE gs_in[1], inout TriangleStream<PS_PARTICLE> gs_out)
{
	ParticleCommonGS (gs_in[0], gs_out, 0.666f, 0.002f);
}


[maxvertexcount (4)]
void ParticleSquareGS (point GS_PARTICLE gs_in[1], inout TriangleStream<PS_PARTICLE> gs_out)
{
	ParticleCommonGS (gs_in[0], gs_out, 0.5f, 0.002f);
}

PS_SPRITE GetSpriteVert (float ofsx, float ofsy, float2 TexCoord)
{
	PS_SPRITE vs_out;

	vs_out.Position = mul (mvpMatrix, float4 ((viewRight * ofsy) + (viewUp * ofsx) + SpriteOrigin, 1.0f));
	vs_out.TexCoord = TexCoord;

	return vs_out;
}

[maxvertexcount (4)]
void SpriteGS (triangle float gs_in[3] : BULLSHIT, inout TriangleStream<PS_SPRITE> gs_out)
{
	gs_out.Append (GetSpriteVert (SpriteXYWH.y, SpriteXYWH.x, float2 (0, 1)));
	gs_out.Append (GetSpriteVert (SpriteXYWH.w, SpriteXYWH.x, float2 (0, 0)));
	gs_out.Append (GetSpriteVert (SpriteXYWH.y, SpriteXYWH.z, float2 (1, 1)));
	gs_out.Append (GetSpriteVert (SpriteXYWH.w, SpriteXYWH.z, float2 (1, 0)));
}
#endif


#ifdef PIXELSHADER
float4 SpritePS (PS_SPRITE ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));
	return float4 (diff.rgb, diff.a * SpriteAlpha);
}

float4 ParticleCirclePS (PS_PARTICLE ps_in) : SV_TARGET0
{
	// procedurally generate the particle dot for good speed and per-pixel accuracy at any scale
	float pAlpha = ps_in.Color.a * (1.0f - dot (ps_in.Offsets, ps_in.Offsets));
	clip (pAlpha); // reject any particles contributing less than zero
	return GetGamma (float4 (ps_in.Color.rgb * pAlpha, pAlpha));
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

