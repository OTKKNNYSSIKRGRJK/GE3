#include"Basic.hlsli"

struct TransformationMatrix {
	float4x4 VP;
};

ConstantBuffer<TransformationMatrix> cb_TransformationMatrix : register(b0);

struct VSInput {
	float4 Position : POSITION0;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

float4 CalcPos(VSInput input_) {
	return mul(input_.Position, cb_TransformationMatrix.VP);
}

VSOutput main(VSInput input_) {
	VSOutput output;
	output.Position = CalcPos(input_);
	output.Color = input_.Color;
	return output;
}