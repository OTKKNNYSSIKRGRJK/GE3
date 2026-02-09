#include"Model.hlsli"

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

struct DIRECTIONAL_LIGHT {
	float3 Color;
	float Intensity;
	float3 Dir;
};
ConstantBuffer<DIRECTIONAL_LIGHT> DirectionalLight : register(b0, space2);

float4 LambertianReflectance(in VSOutput input_, in ConstantBuffer<DIRECTIONAL_LIGHT> light_) {
	float nDotL = saturate(dot(normalize(input_.Normal), -light_.Dir.xyz));
	return float4(
		light_.Color *
		light_.Intensity *
		nDotL,
		1.0f
	);
}

float4 HalfLambertianReflectance(in VSOutput input_, in ConstantBuffer<DIRECTIONAL_LIGHT> light_) {
	float nDotL = saturate(dot(normalize(input_.Normal), -light_.Dir.xyz));
	return float4(
		light_.Color *
		light_.Intensity *
		pow(nDotL * 0.75f + 0.25f, 2.0f),
		1.0f
	);
}

PSOutput main(VSOutput input_) {
	PSOutput output;
	
	float4 texColor = Textures[Materials[ModelIndex].TextureID].Sample(Sampler, input_.TexCoord);
	float4 lightColor = HalfLambertianReflectance(input_, DirectionalLight);
	output.Color = texColor * lightColor * Materials[ModelIndex].Color;
	
	return output;
}