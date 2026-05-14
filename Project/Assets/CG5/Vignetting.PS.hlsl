struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

cbuffer Constants : register(b0) {
	float Scale;
	float Power;
}

Texture2D<float4> OffscreenTexture : register(t0);
SamplerState BilinearClamp : register(s0);

PSOutput main(VSOutput input_) {
	PSOutput output;
	const float4 texColor = OffscreenTexture.Sample(BilinearClamp, input_.TexCoord);
	
	const float2 correct = input_.TexCoord * (1.0f - input_.TexCoord.yx);
	float vignette = correct.x * correct.y * Scale;
	vignette = saturate(pow(vignette, Power));
	
	output.Color = texColor;
	output.Color.rgb *= vignette;
	
	return output;
}