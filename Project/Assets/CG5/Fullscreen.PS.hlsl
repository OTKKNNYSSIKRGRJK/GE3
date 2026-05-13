struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> OffscreenTexture : register(t0);
SamplerState BilinearClamp : register(s0);

PSOutput main(VSOutput input_) {
	PSOutput output;
	float4 texColor = OffscreenTexture.Sample(BilinearClamp, input_.TexCoord);
	output.Color = texColor;
	return output;
}