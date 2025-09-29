#define Space_Tex2D space1

Texture2D<float4> Tex : register(t0, Space_Tex2D);
SamplerState DefaultSampler : register(s0);

struct PSOutput {
	float4 Color : SV_TARGET0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

PSOutput main(VSOutput input_) {
	PSOutput output;
	
	float4 texColor = Tex.Sample(DefaultSampler, input_.TexCoord);
	output.Color = texColor * input_.Color * 2.0f;
	
	return output;
}