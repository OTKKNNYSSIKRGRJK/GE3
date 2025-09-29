struct VIEW_PROJECTION {
	float4x4 Mat;
};

ConstantBuffer<VIEW_PROJECTION> VP : register(b0);

struct WORLD {
	float4x4 Mat;
};
ConstantBuffer<WORLD> World : register(b1);

struct VSInput {
	float4 Position : POSITION0;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

float4 CalcPos(in VSInput input_) {
	return mul(mul(input_.Position, World.Mat), VP.Mat);
}

VSOutput main(VSInput input_) {
	VSOutput output;
	
	output.Position = CalcPos(input_);
	output.TexCoord = input_.TexCoord;
	output.Color = input_.Color;
	return output;
}