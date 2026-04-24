struct VSInput {
	float4 Position : POSITION0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float3 ProjectionSpacePos : POSITION0;
	float3 TexCoord : TEXCOORD0;
};

cbuffer Constants : register(b0) {
	float4x4 WVP;
}

VSOutput main(VSInput input_) {
	VSOutput output;
	output.Position = mul(float4(input_.Position.xyz * 1000.0f, 1.0f), WVP).xyww;
	output.TexCoord = input_.Position.xyz;
	return output;
}