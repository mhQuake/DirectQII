
struct VS_SURFCOMMON {
	float4 Position : POSITION;
	float2 TexCoord : TEXCOORD;
	float2 Lightmap : LIGHTMAP;
	float MapNum : MAPNUM;
	int4 Styles: STYLES;
	float Scroll : SCROLL;
};

struct PS_LIGHTMAPPED {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
	float3 Lightmap : LIGHTMAP;
	nointerpolation float4 Styles: STYLES;
};

struct PS_BASIC {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

struct PS_DRAWTURB {
	float4 Position : SV_POSITION;
	float2 TexCoord0 : TEXCOORD0;
	float2 TexCoord1 : TEXCOORD1;
};

struct VS_DRAWSKY {
	float4 Position : POSITION;
};

struct PS_DRAWSKY {
	float4 Position : SV_POSITION;
	float3 TexCoord : TEXCOORD;
};


float2 GetTextureScroll (VS_SURFCOMMON vs_in)
{
	// ensure that SURF_FLOWING scroll is applied consistently
	return vs_in.TexCoord + float2 (TexScroll, 0.0f) * vs_in.Scroll;
}

#ifdef VERTEXSHADER
PS_BASIC SurfBasicVS (VS_SURFCOMMON vs_in)
{
	PS_BASIC vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.TexCoord = GetTextureScroll (vs_in);

	return vs_out;
}

PS_BASIC SurfAlphaVS (VS_SURFCOMMON vs_in)
{
	PS_BASIC vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.TexCoord = GetTextureScroll (vs_in);

	return vs_out;
}

PS_LIGHTMAPPED SurfLightmapVS (VS_SURFCOMMON vs_in)
{
	PS_LIGHTMAPPED vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.TexCoord = GetTextureScroll (vs_in);
	vs_out.Lightmap = float3 (vs_in.Lightmap, vs_in.MapNum);

	vs_out.Styles = float4 (
		LightStyles.Load (vs_in.Styles.x),
		LightStyles.Load (vs_in.Styles.y),
		LightStyles.Load (vs_in.Styles.z),
		LightStyles.Load (vs_in.Styles.w)
	);

	return vs_out;
}

VS_SURFCOMMON SurfDynamicVS (VS_SURFCOMMON vs_in)
{
	return vs_in;
}

PS_DRAWTURB SurfDrawTurbVS (VS_SURFCOMMON vs_in)
{
	PS_DRAWTURB vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.TexCoord0 = GetTextureScroll (vs_in);
	vs_out.TexCoord1 = vs_in.Lightmap + turbTime;

	return vs_out;
}

PS_DRAWSKY SurfDrawSkyVS (VS_DRAWSKY vs_in)
{
	PS_DRAWSKY vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.Position.z = vs_out.Position.w; // fix the skybox to the far clipping plane
	vs_out.TexCoord = mul (SkyMatrix, vs_in.Position).xyz;

	return vs_out;
}
#endif


#ifdef GEOMETRYSHADER
PS_DYNAMICLIGHT GetEnvmappedVertex (float4 Position, float3 Normal, float2 TexCoord)
{
	PS_DYNAMICLIGHT vs_out;

	vs_out.Position = mul (LocalMatrix, Position);
	vs_out.TexCoord = TexCoord;
	vs_out.LightVector = viewOrigin - Position.xyz; // this isn't really a light vector but it will do
	vs_out.Normal = Normal;

	return vs_out;
}

[maxvertexcount (3)]
void SurfEnvmappedGS (triangle VS_SURFCOMMON gs_in[3], inout TriangleStream<PS_DYNAMICLIGHT> gs_out)
{
	// this is the same normal calculation as QBSP does
	// strictly speaking we should first transform the positions from local space to world space, but in practice all alpha surfs
	// are using world space to begin with, so we don't need to
	float3 Normal = normalize (
		cross (gs_in[0].Position.xyz - gs_in[1].Position.xyz, gs_in[2].Position.xyz - gs_in[1].Position.xyz)
	);

	gs_out.Append (GetEnvmappedVertex (gs_in[0].Position, Normal, GetTextureScroll (gs_in[0])));
	gs_out.Append (GetEnvmappedVertex (gs_in[1].Position, Normal, GetTextureScroll (gs_in[1])));
	gs_out.Append (GetEnvmappedVertex (gs_in[2].Position, Normal, GetTextureScroll (gs_in[2])));
}

[maxvertexcount (3)]
void SurfDynamicGS (triangle VS_SURFCOMMON gs_in[3], inout TriangleStream<PS_DYNAMICLIGHT> gs_out)
{
	// this is the same normal calculation as QBSP does
	float3 Normal = normalize (
		cross (gs_in[0].Position.xyz - gs_in[1].Position.xyz, gs_in[2].Position.xyz - gs_in[1].Position.xyz)
	);

	// output position needs to use the same transform as the prior pass to satisfy invariance rules
	// we inverse-transformed the light position by the entity local matrix so we don't need to transform the normal or the light vector stuff
	gs_out.Append (GenericDynamicVS (gs_in[0].Position, Normal, GetTextureScroll (gs_in[0])));
	gs_out.Append (GenericDynamicVS (gs_in[1].Position, Normal, GetTextureScroll (gs_in[1])));
	gs_out.Append (GenericDynamicVS (gs_in[2].Position, Normal, GetTextureScroll (gs_in[2])));
}
#endif


#ifdef PIXELSHADER
float4 SurfBasicPS (PS_BASIC ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));
	return float4 (diff.rgb, 1.0f);
}

float4 SurfAlphaPS (PS_BASIC ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));
	return float4 (diff.rgb, diff.a * AlphaVal);
}

float4 SurfLightmapPS (PS_LIGHTMAPPED ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));

	float3 lmap = float3 (
		dot (lmap0Texture.Sample (lmapSampler, ps_in.Lightmap), ps_in.Styles),
		dot (lmap1Texture.Sample (lmapSampler, ps_in.Lightmap), ps_in.Styles),
		dot (lmap2Texture.Sample (lmapSampler, ps_in.Lightmap), ps_in.Styles)
	);

	return float4 (diff.rgb * lmap, 1.0f);
}

float4 SurfDrawTurbPS (PS_DRAWTURB ps_in) : SV_TARGET0
{
	float4 diff = GetGamma (mainTexture.SampleGrad (mainSampler, ps_in.TexCoord0 + sin (ps_in.TexCoord1) * 0.125f, ddx (ps_in.TexCoord0), ddy (ps_in.TexCoord0)));
	return float4 (diff.rgb, diff.a * AlphaVal);
}

float4 SurfDrawSkyPS (PS_DRAWSKY ps_in) : SV_TARGET0
{
	// reuses the lightmap sampler because it has the same sampler state as is required here
	return GetGamma (sboxTexture.Sample (lmapSampler, ps_in.TexCoord));
}

float4 SkyNoSkyPS (PS_BASIC ps_in) : SV_TARGET0
{
	return float4 (1, 1, 1, 1);
}
#endif

