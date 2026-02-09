struct PSInput {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	uint InstanceID : INSTID0;
};

struct PSOutput {
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
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

static const float Kernel[3][3] = {
	0.05f, 0.15f, 0.05f,
	0.15f, 0.2f, 0.15f,
	0.05f, 0.15f, 0.05f,
};

static const float2 inv_WH = { 1.0f / 120.0f, 1.0f / 90.0f };

float4 Convolve(Texture2D<float4> tex_, float2 texCoord_, in float kernel_[3][3], float rad_) {
	float4 texColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	[unroll]
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			float2 offset = float2(i - 1, j - 1) * inv_WH * rad_;
			texColor += tex_.Sample(Sampler, texCoord_ + offset) * kernel_[2 - i][2 - j];
		}
	}
	return texColor;
}

float4 Bloom(Texture2D<float4> tex_, float2 texCoord_) {
	float4 ret = tex_.Sample(Sampler, texCoord_);
	float intense = 0.7f;
	
	for (int i = 0; i < 5; ++i) {
		ret += Convolve(tex_, texCoord_, Kernel, 1.0f * i) * (5 - i) * 0.05f * intense;
	}
	//ret += Convolve(texCoord_, Kernel, 3.0f) * 0.8f * intense;
	//ret += Convolve(texCoord_, Kernel, 6.0f) * 0.6f * intense;
	//ret += Convolve(texCoord_, Kernel, 9.0f) * 0.4f * intense;
	//ret += Convolve(texCoord_, Kernel, 12.0f) * 0.2f * intense;
	
	return ret;
}

PSOutput main(in PSInput input_) {
	PSOutput output;
	
	Material material = SRV_Materials[input_.InstanceID];
	
	//float r = SRV_Textures[material.TextureID].Sample(Sampler, input_.TexCoord + 0.005f).r;
	//float g = SRV_Textures[material.TextureID].Sample(Sampler, input_.TexCoord - 0.005f).g;
	//float b = SRV_Textures[material.TextureID].Sample(Sampler, input_.TexCoord).b;
	//float a = SRV_Textures[material.TextureID].Sample(Sampler, input_.TexCoord).a;
	//output.Color = material.RGBA * float4(r, g, b, a);
	
	float4 texColorBloom = Bloom(SRV_Textures[material.TextureID], input_.TexCoord);
	texColorBloom.r *= 2.0f;
	output.Diffuse = texColorBloom * material.RGBA;
	output.Normal = float4(0.5f, 0.5f, 0.0f, 1.0f);
	
	return output;
}