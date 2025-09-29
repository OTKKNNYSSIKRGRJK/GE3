struct PSInput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

PSOutput main(in PSInput input_) {
	PSOutput output;
	
	float4 texColor = Textures[0].Sample(Sampler, input_.TexCoord);
	output.Color = texColor * input_.Color;
	
	return output;
}