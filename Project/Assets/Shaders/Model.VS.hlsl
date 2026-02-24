#include"Model.hlsli"

struct Mat4 {
	float4x4 Mat;
};

StructuredBuffer<float4x4> Worlds : register(t0);
ConstantBuffer<Mat4> VP : register(b1);

struct VSInput {
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

VSOutput main(VSInput input_) {
	VSOutput output;
	
	output.Position = mul(mul(input_.Position, Worlds[ModelIndex]), VP.Mat);
	output.TexCoord = input_.TexCoord;
	output.Normal = mul(input_.Normal, (float3x3) Worlds[ModelIndex]);
	
	return output;
}