#include"Basic.hlsli"

struct PSOutput {
	float4 Color : SV_TARGET0;
};

cbuffer Constants : register(b0, space1) {
	float2 PlayerNDCPos;
	float Time;
	uint IsEnemySuccessfullyAttacking;
}

Texture2D<float4> Textures[] : register(t0, space1);
Texture2D<float> DepthTexture : register(t0, space2);
SamplerState Sampler : register(s0);
static const float Kernel[3][3] = {
	0.05f, 0.05f, 0.05f,
	0.05f, 0.6f, 0.05f,
	0.05f, 0.05f, 0.05f,
};
static const float Kernel2[3][3] = {
	-0.25f, -0.5f, 0.25f,
	-0.5f, 0.0f, 0.5f,
	-0.25f, 0.5f, 0.25f,
};

static const float2 inv_WH = { 1.0f / 1280.0f, 1.0f / 720.0f };

float4 Convolve(uint texID_, in float2 texCoord_, in float kernel_[3][3], float rad_) {
	float4 texColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	[unroll]
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			float2 offset = float2(i - 1, j - 1) * inv_WH * rad_;
			texColor += Textures[texID_].Sample(Sampler, texCoord_ + offset) * kernel_[2 - i][2 - j];
		}
	}
	return texColor;
}

float4 Bloom(uint texID_, in float2 texCoord_, in float2 offset_, float len_) {
	//float4 ret = Textures[64].Sample(g_Sampler, texCoord_);
	float4 ret = 0.0f;
	//float intense = abs(sin(Time * 50.0f) * 1.0f) + 0.5f;
	float intense = .7f * exp(-Time * 7.0f);
	
	[unroll]
	for (int i = 1; i < 5; ++i) {
		ret += Convolve(texID_, texCoord_, Kernel, i * 0.5f) * (0.6f - i * 0.025f) * intense;
	}
	//ret += Convolve(texCoord_, Kernel, 10.0f) * 0.6f * intense;
	//ret += Convolve(texCoord_, Kernel, 15.0f) * 0.4f * intense;
	//ret += Convolve(texCoord_, Kernel, 20.0f) * 0.2f * intense;
	
	return ret;
}

float4 Distort(uint texID_, in float2 texCoord_, in float2 offset_, float len_) {
	//float2 offset = float2(
	//	cos(EnemyPos.x * .72f) * sin(texCoord_.y * 7.5f) * 0.01f,
	//	sin(EnemyPos.y * .75f) * sin(texCoord_.x * 7.2f) * 0.01f
	//);
	//float d = exp(-len_ * 0.5f - Time * 1.3f) * 0.5f;
	float d1 =
		exp(-(len_ * 0.5f + Time * 7.0f)) *
		sin(len_ * 2.0f + Time * 25.0f) *
		0.34f;
	float d2 =
		exp(-(len_ * 0.7f + Time * 6.0f)) *
		sin(len_ * 3.0f + Time * 20.0f) *
		0.35f;
	float d3 =
		exp(-(len_ * 0.9f + Time * 5.0f)) *
		sin(len_ * 5.0f + Time * 15.0f) *
		0.36f;
	
	return float4(
		Textures[texID_].Sample(Sampler, texCoord_ + d1 * offset_).r,
		Textures[texID_].Sample(Sampler, texCoord_ + d2 * offset_).g,
		Textures[texID_].Sample(Sampler, texCoord_ + d3 * offset_).b,
		1.0f
	);
	//return Textures[texID_].Sample(Sampler, texCoord_ + d * offset);
}

PSOutput main(VSOutput input_) {
	PSOutput output;
	if (IsEnemySuccessfullyAttacking) {
		float2 offset = (PlayerNDCPos.xy * float2(1.0f, -1.0f) + float2(1.0f, 1.0f)) * 0.5f - input_.TexCoord.xy;
		offset *= 0.5f;
		float len = length(offset);
		
		float4 distort = Distort(input_.TexID, input_.TexCoord, offset, len);
		float4 bloom = Bloom(input_.TexID, input_.TexCoord, offset, len);
		output.Color = distort + bloom;
	}
	else {
		float4 texColor = input_.TexID < 1024U ? Textures[input_.TexID].Sample(Sampler, input_.TexCoord) : float4(1.0f, 1.0f, 1.0f, 1.0f);
		output.Color = texColor * input_.Color;
	}
	//Texture2D<float> depthTex = ResourceDescriptorHeap[DepthTexID];
	////float depth = depthTex.Sample(Sampler, input_.TexCoord);
	////output.Color.rgb = (depth - 0.99f) * 10.0f;
	return output;
}