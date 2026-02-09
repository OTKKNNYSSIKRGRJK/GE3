struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

struct MATERIAL {
	float4 Color;
	uint TextureID;
};

#define SPACE_MATERIAL space3

uint ModelIndex : register(b0);
ConstantBuffer<MATERIAL> Materials[] : register(b0, SPACE_MATERIAL);