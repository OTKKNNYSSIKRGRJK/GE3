struct VSOutput {
	float4 Position : SV_Position;
	float3 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
};

TextureCube<float4> Texture : register(t0);
SamplerState Sampler : register(s0);

PSOutput main(VSOutput input_) {
	PSOutput output;
	float4 texColor = Texture.Sample(Sampler, input_.TexCoord);
	output.Diffuse = texColor;
	output.Normal = float4(1.0f, 0.0f, 0.0f, 0.0f);
	return output;
}