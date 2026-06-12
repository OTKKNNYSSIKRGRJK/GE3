struct VSOutput {
	float4 Pos : SV_Position;
	float3 WorldPos : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

struct PSOutput {
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 Factors0 : SV_TARGET2;
	//float4 Specular : SV_TARGET3;
};

Texture2D<float4> Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

struct MATERIAL {
	float4 Color;
	uint ID_DiffuseMap;
	uint ID_SpecularMap;
	uint ID_NormalMap;
};
#define SPACE_MATERIAL space3
ConstantBuffer<MATERIAL> Material : register(b0, SPACE_MATERIAL);

TextureCube<float4> SRV_EnvironmentMap : register(t0, space17);
cbuffer Parameter_Space17Slot0 : register(b0, space17) {
	float3 WorldPos_Camera;
}

float4 CalcEnv(
	in float3 worldPos_,
	in float3 norm_
) {
	const float3 dir_EyeToTarget = normalize(worldPos_ - WorldPos_Camera.xyz);
	
	const float3 refl = reflect(dir_EyeToTarget, norm_);
	float4 envColor = SRV_EnvironmentMap.Sample(Sampler, refl);
	envColor.rgb *= float3(0.02f, 0.04f, 0.05f);
	
	return envColor;
}

PSOutput main(VSOutput input_) {
	PSOutput output;
	
	
	const float4 diffuseColor = Textures[Material.ID_DiffuseMap].Sample(Sampler, input_.TexCoord);
	output.Diffuse = diffuseColor * Material.Color;
	
	const float4 envColor = CalcEnv(input_.WorldPos.xyz, input_.Normal);
	output.Diffuse = output.Diffuse * 0.1f + envColor * 0.9f;
	
	const float3 normal = float3(0.0f, 0.0f, 1.0f);
	output.Normal = float4(normal * 0.5f + 0.5f, 1.0f);
	// * Bleeding
	output.Factors0.r = abs(normal.z);
	// * Edge Density
	output.Factors0.g = abs(cos(input_.Pos * 0.01f));
	// * Depth
	//output.Factors0.b = input_.Pos.z / input_.Pos.w;
	
	return output;
}