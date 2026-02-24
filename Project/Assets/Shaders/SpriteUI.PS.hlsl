struct PSInput {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	uint InstanceID : INSTID0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

struct Material {
	float4 RGBA;
	uint TextureID;
};

StructuredBuffer<Material> SRV_Materials : register(t0, space0);

Texture2D<float4> SRV_Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

PSOutput main(in PSInput input_) {
	PSOutput output;
	
	Material material = SRV_Materials[input_.InstanceID];
	
	output.Color = SRV_Textures[material.TextureID].Sample(Sampler, input_.TexCoord) * material.RGBA;
	
	return output;
}