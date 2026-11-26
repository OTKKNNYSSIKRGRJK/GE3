#include"Common.hlsli"

Texture2D<float4> SRV_Map_Trampling : SLOT_SAV_MAP_TRAMPLING;
Texture2D<float4> SRV_Map_Tint : SLOT_SRV_MAP_TINT;
Texture2D<float4> SRV_GrassBladeAlbedo : register(t0, SPACE_IMGTEX);

SamplerState BilinearWrap : register(s0);

static const float3 LightColor = float3(1.0f, 0.5f, 0.875f);
static const float LightIntensity = 3.0f;
static const float3 LightDir = normalize(float3(-1.0f, -1.0f, 0.0f));

float4 HalfLambertianReflectance(in GrassBlade::PSInput input_) {
	float dot_N_L = saturate(dot(normalize(input_.Normal), -LightDir));
	return float4(
		LightColor *
		LightIntensity *
		pow(dot_N_L * 0.75f + 0.25f, 2.0f),
		1.0f
	);
}

GrassBlade::PSOutput main(GrassBlade::PSInput input_) {
	GrassBlade::PSOutput output;
	
	float4 albedo = SRV_GrassBladeAlbedo.Sample(BilinearWrap, input_.TexCoord);
	float4 blend0 = SRV_Map_Tint.SampleLevel(BilinearWrap, input_.TexCoord_Grassland, 0.0f);
	float4 blend1 = SRV_Map_Tint.Sample(BilinearWrap, input_.TexCoord * float2(4.0f, 1.0f));
	float4 args_Trample = SRV_Map_Trampling.SampleLevel(BilinearWrap, input_.TexCoord_Grassland, 0.0f);
	
	output.Color = (albedo * 0.875f + blend0 * 0.125f) * (albedo * 0.95f + blend1 * 0.05f) * blend0;
	output.Color.xyz += float3(0.03f, 0.025f, 0.0f) * (1.0f - args_Trample.w);
	output.Color *= HalfLambertianReflectance(input_);
	output.Color.z *= 7.5f;
	output.Color.xyz += pow(saturate(input_.Position.z - 0.994f), 2.0f) * float3(-150.0f, 750.0f, 900.0f);
	
	return output;
}