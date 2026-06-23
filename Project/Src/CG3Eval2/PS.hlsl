#include"Common.hlsli"

Texture2D<float4> Arr_SRV_Textures[] : register(t0, SPACE_IMGTEX);

SamplerState BilinearWrap : register(s0);

cbuffer DissolveConstants : register(b0) {
	uint MaskTextureID;
	float Threshold;
};

namespace Grayscale {
	float BT709(in float3 rgb_) {
		return dot(rgb_, float3(0.2125f, 0.7154f, 0.0721f));
	}
}

CG3::PSOutput main(CG3::PSInput input_) {
	CG3::PSOutput output;
	
	// input_.TexID : [0.0f, 1.0f]
	
	const uint texID0 = uint(max(floor(input_.TexID), 0.0f));
	const uint texID1 = uint(min(ceil(input_.TexID), 1.0f));
	const float4 texColor0 = Arr_SRV_Textures[texID0].Sample(BilinearWrap, input_.TexCoord);
	const float4 texColor1 = Arr_SRV_Textures[texID1].Sample(BilinearWrap, input_.TexCoord);
	const float t = input_.TexID * input_.TexID;
	const float4 texColor = (texColor0 * (1.0f - t) + texColor1 * t) * texColor0;
	output.Albedo = texColor;
	
	const float4 dissolveTexColor = Arr_SRV_Textures[MaskTextureID].Sample(BilinearWrap, input_.TexCoord);
	const float luminance = Grayscale::BT709(dissolveTexColor.rgb);
	if (luminance < Threshold) {
		discard;
	}
	
	output.Normal = float4(normalize(input_.Normal) * 0.5f + 0.5f, 1.0f);
	
	return output;
}