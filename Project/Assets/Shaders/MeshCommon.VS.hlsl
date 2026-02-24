#include"MeshCommon.hlsli"

struct Mat4 {
	float4x4 Mat;
};

StructuredBuffer<float3> PositionArrays[] : register(t0, space32);
StructuredBuffer<float2> TexCoordArrays[] : register(t0, space33);
StructuredBuffer<float3> NormalArrays[] : register(t0, space34);
StructuredBuffer<float3> TangentArrays[] : register(t0, space35);
StructuredBuffer<float4x4> Worlds : register(t0, space16);
ConstantBuffer<Mat4> VP : register(b0, space16);

struct VSInput {
	uint Index_Position : IDX_POSITION0;
	uint Index_TexCoord : IDX_TEXCOORD0;
	uint Index_Normal : IDX_NORMAL0;
	uint Index_Tangent : IDX_TANGENT0;
};

VSOutput main(VSInput input_) {
	VSOutput output;
	
	float3 position = PositionArrays[MeshIndex][input_.Index_Position];
	float2 texCoord = TexCoordArrays[MeshIndex][input_.Index_TexCoord];
	float3 normal = NormalArrays[MeshIndex][input_.Index_Normal];
	
	output.Position = mul(mul(float4(position, 1.0f), Worlds[MeshIndex]), VP.Mat);
	output.TexCoord = texCoord;
	output.Normal = mul(normal, (float3x3) Worlds[MeshIndex]);
	
	return output;
}