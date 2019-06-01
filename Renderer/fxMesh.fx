

struct VS_MESH {
	uint4 PrevTriVertx: PREVTRIVERTX;
	uint4 CurrTriVertx: CURRTRIVERTX;
	float2 TexCoord: TEXCOORD;
};

struct PS_MESH {
	float4 Position: SV_POSITION;
	float2 TexCoord: TEXCOORD;
	float3 Normal : NORMAL;
};

#ifdef VERTEXSHADER
float4 MeshLerpPosition (VS_MESH vs_in)
{
	return float4 (Move + vs_in.CurrTriVertx.xyz * FrontV + vs_in.PrevTriVertx.xyz * BackV, 1.0f);
}

float3 MeshLerpNormal (VS_MESH vs_in)
{
	// note: this is the correct order for normals; check the light on the hyperblaster v_ model, for example;
	// with the opposite order it flickers pretty badly as the model animates; with this order it's nice and solid
	float3 n1 = LightNormals.Load (vs_in.CurrTriVertx.w).xyz;
	float3 n2 = LightNormals.Load (vs_in.PrevTriVertx.w).xyz;
	return normalize (lerp (n1, n2, BackLerp));
}

PS_MESH MeshLightmapVS (VS_MESH vs_in)
{
	PS_MESH vs_out;

	vs_out.Position = mul (LocalMatrix, MeshLerpPosition (vs_in));
	vs_out.TexCoord = vs_in.TexCoord;
	vs_out.Normal = MeshLerpNormal (vs_in);

	return vs_out;
}

float4 MeshPowersuitVS (VS_MESH vs_in) : SV_POSITION
{
	return mul (LocalMatrix, MeshLerpPosition (vs_in) + float4 (MeshLerpNormal (vs_in) * PowersuitScale, 0.0f));
}

PS_DYNAMICLIGHT MeshDynamicVS (VS_MESH vs_in)
{
	return GenericDynamicVS (MeshLerpPosition (vs_in), MeshLerpNormal (vs_in), vs_in.TexCoord);
}
#endif


#ifdef PIXELSHADER
float4 MeshLightmapPS (PS_MESH ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));
	float shadedot = dot (normalize (ps_in.Normal), ShadeVector);
	float3 lmap = ShadeLight * max (shadedot + 1.0f, (shadedot * 0.2954545f) + 1.0f);

	return float4 (diff.rgb * Desaturate (lmap), diff.a * AlphaVal);
}

float4 MeshFullbrightPS (PS_MESH ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));
	return float4 (diff.rgb, diff.a * AlphaVal);
}

float4 MeshPowersuitPS (float4 Position: SV_POSITION) : SV_TARGET0
{
	return GetGamma (float4 (ShadeLight, AlphaVal));
}
#endif

