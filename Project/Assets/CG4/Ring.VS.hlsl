struct VSInput {
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

cbuffer Constants : register(b0) {
	float4x4 WorldToProjective;
	float4x4 ViewToWorld;
}

VSOutput main(VSInput input_) {
	VSOutput output;
	output.Position = float4(mul(input_.Position.xyz, (float3x3) ViewToWorld), 1.0f);
	output.Position = mul(output.Position, WorldToProjective);
	output.TexCoord = input_.TexCoord;
	return output;
}