struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

cbuffer CBV_Gradients : register(b0) {
	float4 Gradients[256];
}

Texture2D<float4> OffscreenTexture : register(t0);
SamplerState BilinearClamp : register(s0);

namespace Grayscale {
	float BT709(in float3 rgb_) {
		return dot(rgb_, float3(0.2125f, 0.7154f, 0.0721f));
	}
}

float4 Gradient(float brightness_) {
	brightness_ = saturate(brightness_);
	brightness_ *= 255.0f;
	const int brightness_Floor = (int) floor(brightness_ );
	const int brightness_Ceil = (int) ceil(brightness_);
	const float t = frac(brightness_);
	return Gradients[brightness_Floor] * (1.0f - t) + Gradients[brightness_Ceil] * t;
}

PSOutput main(VSOutput input_) {
	PSOutput output;
	const float4 texColor = OffscreenTexture.Sample(BilinearClamp, input_.TexCoord);
	output.Color = texColor;
	const float brightness = Grayscale::BT709(output.Color.rgb);
	output.Color.rgb = Gradient(brightness).rgb;
	return output;
}