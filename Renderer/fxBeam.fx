
#ifdef VERTEXSHADER
float4 BeamVS (float4 Position: POSITION) : SV_POSITION
{
	return mul (LocalMatrix, Position);
}
#endif


#ifdef PIXELSHADER
float4 BeamPS (float4 Position: SV_POSITION) : SV_TARGET0
{
	return GetGamma (float4 (ShadeColor, AlphaVal));
}
#endif

