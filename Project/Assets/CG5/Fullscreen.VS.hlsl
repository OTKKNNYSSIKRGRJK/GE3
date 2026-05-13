struct VSInput {
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

VSOutput main(VSInput input_) {
	VSOutput output;
	output.Position = input_.Position;
	output.TexCoord = input_.TexCoord;
	return output;
}