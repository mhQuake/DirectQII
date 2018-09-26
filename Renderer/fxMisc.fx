
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

struct PS_DRAWSPRITE {
	float4 Position : SV_POSITION;
	float4 Color : COLOUR;
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
PS_DRAWSPRITE SpriteVS (VS_QUADBATCH vs_in)
{
	PS_DRAWSPRITE vs_out;

	vs_out.Position = mul (mvpMatrix, vs_in.Position);
	vs_out.Color = vs_in.Color;
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}

GS_PARTICLE ParticleVS (VS_PARTICLE vs_in)
{
	GS_PARTICLE vs_out;

	// move the particle in a framerate-independent manner
	// p->org[0] + (p->vel[0] + p->accel[0] * time) * time;
	vs_out.Origin = vs_in.Origin + (vs_in.Velocity + vs_in.Acceleration * vs_in.Time) * vs_in.Time;

	// copy over colour
	vs_out.Color = float4 (QuakePalette.Load (int3 (vs_in.Color.x, 0, 0)).rgb, vs_in.Alpha);

	return vs_out;
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


#ifdef GEOMETRYSHADER
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
	float ScaleUp = 1.0f + dot (gs_in.Origin - viewOrigin, viewForward) * HackUp;

	gs_out.Append (GetParticleVert (gs_in, float2 (-1, -1), ScaleUp * TypeScale));
	gs_out.Append (GetParticleVert (gs_in, float2 (-1,  1), ScaleUp * TypeScale));
	gs_out.Append (GetParticleVert (gs_in, float2 ( 1, -1), ScaleUp * TypeScale));
	gs_out.Append (GetParticleVert (gs_in, float2 ( 1,  1), ScaleUp * TypeScale));
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
	return GetGamma (float4 (ps_in.Color.rgb, saturate (ps_in.Color.a * (1.0f - dot (ps_in.Offsets, ps_in.Offsets)))));
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

