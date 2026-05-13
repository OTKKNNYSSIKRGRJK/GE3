struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> OffscreenTexture : register(t0);
SamplerState BilinearClamp : register(s0);

namespace Grayscale {
	float BT709(in float3 rgb_) {
		return dot(rgb_, float3(0.2125f, 0.7154f, 0.0721f));
	}
}

PSOutput main(VSOutput input_) {
	PSOutput output;
	const float4 texColor = OffscreenTexture.Sample(BilinearClamp, input_.TexCoord);
	output.Color = texColor;
	output.Color.rgb = Grayscale::BT709(output.Color.rgb);
	return output;
}