
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

struct PS_PARTICLE {
	float4 Position : SV_POSITION;
	float4 Color : COLOUR;
	float2 Offsets : OFFSETS;
};


#ifdef VERTEXSHADER
GS_PARTICLE ParticleVS (VS_PARTICLE vs_in)
{
	GS_PARTICLE vs_out;

	// move the particle in a framerate-independent manner
	vs_out.Origin = vs_in.Origin + (vs_in.Velocity + vs_in.Acceleration * vs_in.Time) * vs_in.Time;

	// copy over colour
	vs_out.Color = float4 (QuakePalette.Load (vs_in.Color).rgb, vs_in.Alpha);

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
	// frustum cull the particle
	if (dot (gs_in.Origin, frustum0.xyz) - frustum0.w <= -1) return;
	if (dot (gs_in.Origin, frustum1.xyz) - frustum1.w <= -1) return;
	if (dot (gs_in.Origin, frustum2.xyz) - frustum2.w <= -1) return;
	if (dot (gs_in.Origin, frustum3.xyz) - frustum3.w <= -1) return;

	// hack a scale up to keep particles from disapearing
	float ScaleUp = (1.0f + dot (gs_in.Origin - viewOrigin, viewForward) * HackUp) * TypeScale;

	// and write it out
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
#endif


#ifdef PIXELSHADER
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
#endif

