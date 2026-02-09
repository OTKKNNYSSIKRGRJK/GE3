#include"Basic.hlsli"

float4x4 VP : register(b0);

struct VSInput {
	float4 Position : POSITION0;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	uint TexID : TEXID0;
};

VSOutput main(VSInput input_) {
	VSOutput output;
	output.Position = mul(input_.Position, VP);
	output.Color = input_.Color;
	output.TexCoord = input_.TexCoord;
	output.TexID = input_.TexID;
	return output;
}