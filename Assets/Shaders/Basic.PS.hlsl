#include"Basic.hlsli"

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

PSOutput main(VSOutput input_) {
	PSOutput output;
	//uint id = input_.InstanceID;
	
	//float4 texColor = Textures[0].Sample(Sampler, input_.Position.xy * 0.001f);
	output.Color = input_.Color;
	
	return output;
}