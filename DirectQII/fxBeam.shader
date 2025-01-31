
#ifdef VERTEXSHADER
float4 BeamVS (uint vertexId : SV_VertexID) : SV_POSITION
{
	float angle = 2.0f * M_PI * (float) (vertexId / 2) / (float) beamdetail;
	float4 Position = float4 (sin (angle) * 0.5f, cos (angle) * 0.5f, (vertexId + 1) % 2, 1);

	return mul (LocalMatrix, Position);
}
#endif


#ifdef PIXELSHADER
float4 BeamPS (float4 Position: SV_POSITION) : SV_TARGET0
{
	return GetGamma (float4 (ShadeColor, AlphaVal));
}
#endif

