struct PSInput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> Texture : register(t0, space1);
SamplerState Sampler : register(s0);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

PSOutput main(in PSInput input_) {
	PSOutput output;
	
	output.Color =
		Texture.Sample(Sampler, input_.TexCoord) * input_.Color +
		float4(abs(input_.TexCoord - 0.5f) * 0.25f, 0.0f, 0.0f);
	output.Color *= 1.5f;
	
	return output;
}