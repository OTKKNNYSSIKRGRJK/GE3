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
}

cbuffer Constants1 : register(b1) {
	float Kernel[_KERNEL_MAXWIDTH_ * _KERNEL_MAXHEIGHT_];
}

cbuffer Constants2 : register(b2) {
	float2 Offsets[_KERNEL_MAXWIDTH_ * _KERNEL_MAXHEIGHT_];
}

Texture2D<float4> OffscreenTexture : register(t0);
SamplerState BilinearClamp : register(s0);

float3 Convolve(in float2 texCoord_) {
	float3 ret = { 0.0f, 0.0f, 0.0f };
	for (uint y = 0; y < KernelHeight; ++y) {
		for (uint x = 0; x < KernelWidth; ++x) {
			const uint idx = y * KernelWidth + x;
			const float2 texCoord = texCoord_ + Offsets[idx] * UVStepSize;
			const float3 fetchColor = OffscreenTexture.Sample(BilinearClamp, texCoord).rgb;
			ret += fetchColor * Kernel[idx];
		}
	}
	return ret;
}

PSOutput main(VSOutput input_){
	PSOutput output;
	
	output.Color.rgb = Convolve(input_.TexCoord);
	output.Color.a = 1.0f;
	
	return output;
}