struct TRANSFORM {
	float4x4 Mat;
};

ConstantBuffer<TRANSFORM> LocalToWorld : register(b0, space0);
ConstantBuffer<TRANSFORM> WorldToNDC : register(b1, space0);

struct VSInput {
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

VSOutput main(in VSInput input_) {
	VSOutput output;
	output.Position = mul(mul(input_.Position, LocalToWorld.Mat), WorldToNDC.Mat);
	output.TexCoord = input_.TexCoord;
	return output;
}