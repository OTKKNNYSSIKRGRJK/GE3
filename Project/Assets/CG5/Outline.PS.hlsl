struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

#define _KERNEL_MAXWIDTH_ 9
#define _KERNEL_MAXHEIGHT_ 9

cbuffer Constants0 : register(b0) {
	float2 UVStepSize;
	uint KernelWidth;
	uint KernelHeight;
	float4x4 ProjetiveToView;
	float OutlineLuminanceFactor;
	float OutlineDepthFactor;
	float OutlineSaturateFactor;
	float OutlinePowerFactor;
	float4 OutlineColor;
	float OutlineLuminanceSize;
	float OutlineDepthSize;
}

cbuffer Offsets : register(b1) {
	float2 Offsets[_KERNEL_MAXWIDTH_ * _KERNEL_MAXHEIGHT_];
}

cbuffer Kernel0 : register(b0, space1) {
	float PrewittH[_KERNEL_MAXWIDTH_ * _KERNEL_MAXHEIGHT_];
}
cbuffer Kernel1 : register(b1, space1) {
	float PrewittV[_KERNEL_MAXWIDTH_ * _KERNEL_MAXHEIGHT_];
}

Texture2D<float4> OffscreenTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
SamplerState BilinearClamp : register(s0);
SamplerState PointClamp : register(s1);

namespace Luminance {
	float BT709(in float3 rgb_) {
		return dot(rgb_, float3(0.2125f, 0.7154f, 0.0721f));
	}
}

float Convolve_Luminance(in float2 texCoord_) {
	float ret = 0.0f;
	for (uint y = 0; y < KernelHeight; ++y) {
		for (uint x = 0; x < KernelWidth; ++x) {
			const uint idx = y * KernelWidth + x;
			const float2 texCoord = texCoord_ + Offsets[idx] * UVStepSize * OutlineLuminanceSize;
			const float3 fetchColor = OffscreenTexture.Sample(BilinearClamp, texCoord).rgb;
			const float luminance = Luminance::BT709(fetchColor);
			ret += luminance * PrewittH[idx];
			ret += luminance * PrewittV[idx];
		}
	}
	return abs(ret);
}

float Convolve_Depth(in float2 texCoord_) {
	float ret = 0.0f;
	for (uint y = 0; y < KernelHeight; ++y) {
		for (uint x = 0; x < KernelWidth; ++x) {
			const uint idx = y * KernelWidth + x;
			const float2 texCoord = texCoord_ + Offsets[idx] * UVStepSize * OutlineDepthSize;
			const float ndcDepth = DepthTexture.Sample(PointClamp, texCoord);
			const float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), ProjetiveToView);
			const float viewSpaceDepth = viewSpace.z * rcp(viewSpace.w);
			ret += viewSpaceDepth * PrewittH[idx];
			ret += viewSpaceDepth * PrewittV[idx];
		}
	}
	return abs(ret);
}

PSOutput main(VSOutput input_) {
	PSOutput output;
	
	const float outline =
		Convolve_Luminance(input_.TexCoord) * OutlineLuminanceFactor +
		Convolve_Depth(input_.TexCoord) * OutlineDepthFactor;
	const float outlineAlpha = 1.0f - pow(saturate((1.0f - outline) * OutlineSaturateFactor), OutlinePowerFactor);
	
	const float3 color = OffscreenTexture.Sample(BilinearClamp, input_.TexCoord).rgb;
	
	output.Color.rgb = color;
	output.Color.rgb *= (1.0f - outlineAlpha) + OutlineColor.rgb * outlineAlpha;
	output.Color.a = 1.0f;
	
	return output;
}