#include"MeshCommon.hlsli"

struct PSOutput {
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
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
ConstantBuffer<MATERIAL> Materials[] : register(b0, SPACE_MATERIAL);

PSOutput main(VSOutput input_) {
	PSOutput output;
	
	float4 diffuseColor = Textures[Materials[MeshIndex].ID_DiffuseMap].Sample(Sampler, input_.TexCoord);
	output.Diffuse = diffuseColor * Materials[MeshIndex].Color;
	output.Normal = float4(normalize(input_.Normal.xyz) * 0.5f + 0.5f, 1.0f);
	
	return output;
}