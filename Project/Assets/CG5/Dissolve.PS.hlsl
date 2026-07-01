struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

cbuffer Constants0 : register(b0) {
	float4 MaskedColor;
	float4 EdgeColor;
	float EdgeThreshold_MIN;
	float EdgeThreshold_MAX;
}

Texture2D<float4> OffscreenTexture : register(t0);
Texture2D<float4> MaskTexture : register(t1);
SamplerState BilinearClamp : register(s0);

namespace Grayscale {
	float BT709(in float3 rgb_) {
		return dot(rgb_, float3(0.2125f, 0.7154f, 0.0721f));
	}
}

PSOutput main(VSOutput input_) {
	const float4 texColor = OffscreenTexture.Sample(BilinearClamp, input_.TexCoord);
	
	const float4 maskColor = MaskTexture.Sample(BilinearClamp, input_.TexCoord);
	const float maskLuminance = Grayscale::BT709(maskColor.rgb);
	
	const float threshold = smoothstep(EdgeThreshold_MIN, EdgeThreshold_MAX, maskLuminance);
	const float4 maskedColor = lerp(MaskedColor, float4(1.0f, 1.0f, 1.0f, 1.0f), threshold);
	const float edge = 1.0f - 2.0f * abs(0.5f - threshold);
	const float4 edgeColor = edge * EdgeColor;
	
	PSOutput output;
	
	output.Color = float4(texColor.rgb, 1.0f);
	output.Color.rgb *= maskedColor.rgb;
	output.Color.rgb += edgeColor.rgb;
	
	return output;
}