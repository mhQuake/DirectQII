
// -----------------------------------------------------------------------------------------------------------------------------------------------------------------
// CBUFFERS
// cBuffers are available to all shader stages and are divided by update frequency and usage

cbuffer cbDrawPerFrame : register(b0) {
	matrix orthoMatrix : packoffset(c0);
	float v_gamma : packoffset(c4.x);
	float v_contrast : packoffset(c4.y);
	float2 ConScale : packoffset(c4.z);
};

cbuffer cbMainPerFrame : register(b1) {
	matrix mvpMatrix : packoffset(c0);
	float3 viewOrigin : packoffset(c4);
	float3 viewForward : packoffset(c5);
	float3 viewRight : packoffset(c6);
	float3 viewUp : packoffset(c7);
	float4 vBlend : packoffset(c8);
	float2 WarpTime : packoffset(c9.x);
	float TexScroll : packoffset(c9.z);
	float turbTime : packoffset(c9.w);
	
	// in case we ever need these...
	float RefdefX : packoffset(c10.x);
	float RefdefY : packoffset(c10.y);
	float RefdefW : packoffset(c10.z);
	float RefdefH : packoffset(c10.w);

	matrix SkyMatrix : packoffset(c11);

	float4 frustum0 : packoffset(c15);
	float4 frustum1 : packoffset(c16);
	float4 frustum2 : packoffset(c17);
	float4 frustum3 : packoffset(c18);
};

cbuffer cbPerObject : register(b2) {
	matrix LocalMatrix : packoffset(c0);
	float3 ShadeColor : packoffset(c4.x); // for non-mesh objects that use a colour (beams/null models/etc)
	float AlphaVal : packoffset(c4.w);
};

cbuffer cbPerMesh : register(b3) {
	float3 ShadeLight : packoffset(c0);
	float3 ShadeVector : packoffset(c1);
	float3 Move : packoffset(c2);
	float3 FrontV : packoffset(c3);
	float3 BackV : packoffset(c4);
	float PowersuitScale : packoffset(c5.x);
	float BackLerp : packoffset(c5.y);
};

cbuffer cbPerLight : register(b4) {
	float3 LightOrigin : packoffset(c0.x);
	float LightRadius : packoffset(c0.w);
	float3 LightColour : packoffset(c1.x);
};


// common to mesh and surf
struct PS_DYNAMICLIGHT {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
	float3 LightVector : LIGHTVECTOR;
	float3 Normal : NORMAL;
};


static const float M_PI = 3.14159265f;


#ifdef VERTEXSHADER
Buffer<float> LightStyles : register(t0);
Buffer<float4> LightNormals : register(t1);
Buffer<float4> QuakePalette : register(t2);
#endif


// this ensures that we use the same dynamic calcs on surfs and meshs
// outside of the #ifdef guard because it's used by the VS for meshs and the GS for surfs
PS_DYNAMICLIGHT GenericDynamicVS (float4 Position, float3 Normal, float2 TexCoord)
{
	PS_DYNAMICLIGHT vs_out;

	vs_out.Position = mul (LocalMatrix, Position);
	vs_out.TexCoord = TexCoord;
	vs_out.LightVector = LightOrigin - Position.xyz;
	vs_out.Normal = Normal;

	return vs_out;
}


#ifdef PIXELSHADER
// sampler slots
sampler mainSampler : register(s0);		// main sampler used for most objects, wrap mode, linear filter, max anisotropy
sampler lmapSampler : register(s1);		// lightmap sampler, always linear, clamp mode, no mips, no anisotropy; also reused for the skybox
sampler warpSampler : register(s2);		// underwater and other warps, always linear, wrap mode, no mips, no anisotropy
sampler drawSampler : register(s3);		// used for the 2d render; linear sampled, clamp mode, no mips, no anisotropy
sampler cineSampler : register(s4);		// used for cinematics; linear sampled, clamp-to-border mode, no mips, no anisotropy

// texture slots
Texture2D<float4> mainTexture : register(t0);	// main diffuse texture on most objects
Texture2DArray<float4> lmap0Texture : register(t1);	// lightmap styles 0/1/2/3
Texture2DArray<float4> lmap1Texture : register(t2);	// lightmap styles 0/1/2/3
Texture2DArray<float4> lmap2Texture : register(t3);	// lightmap styles 0/1/2/3
TextureCube<float4> sboxTexture : register(t4);	// sky box texture
Texture2D<float4> warpTexture : register(t5);	// underwater warp noise texture
Texture2DArray<float4> charTexture : register(t6);	// characters and numbers


// faster than a full-screen gamma pass over the scene as a post-process, and more flexible than texture gamma
float4 GetGamma (float4 colorin)
{
	// gamma is not applied to alpha
	// this isn't actually "contrast" but it's consistent with what other engines do
	return float4 (pow (max (colorin.rgb * v_contrast, 0.0f), v_gamma), colorin.a);
}


// dlights for all surf and mesh objects, keeping dynamic lighting consistent across object types; this is based on the lighting performed
// by qrad.exe and light.exe rather than any of the dynamic lighting systems in the original fixed-pipeline code in the engine.
float4 GenericDynamicPS (PS_DYNAMICLIGHT ps_in) : SV_TARGET0
{
	// this clip is sufficient to exclude unlit portions; Add below may still bring it to 0
	// but in practice it's rare and it runs faster without a second clip
	clip ((LightRadius * LightRadius) - dot (ps_in.LightVector, ps_in.LightVector));

	// reading the diffuse texture early so that it should interleave with some ALU ops
	float4 diff = GetGamma (mainTexture.Sample (mainSampler, ps_in.TexCoord));

	// this calc isn't correct per-theory but it matches with the calc used by light.exe and qrad.exe
	// at this stage we don't adjust for the overbright range; that will be done via the "intensity" cvar in the C code
	float Angle = ((dot (normalize (ps_in.Normal), normalize (ps_in.LightVector)) * 0.5f) + 0.5f) / 256.0f;

	// using our own custom attenuation, again it's not correct per-theory but matches the Quake tools
	float Add = max ((LightRadius - length (ps_in.LightVector)) * Angle, 0.0f);

	return float4 (diff.rgb * LightColour * Add, 0.0f);
}
#endif

