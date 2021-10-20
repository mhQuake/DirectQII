
struct PS_NULL {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
};

#ifdef VERTEXSHADER
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
#endif


#ifdef PIXELSHADER
float4 NullPS (PS_NULL ps_in) : SV_TARGET0
{
	float shadedot = dot (normalize (ps_in.Normal), normalize (float3 (1.0f, 1.0f, 1.0f)));
	return GetGamma (float4 (ShadeColor * max (shadedot + 1.0f, (shadedot * 0.2954545f) + 1.0f), AlphaVal));
}
#endif

