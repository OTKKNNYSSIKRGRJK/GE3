struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

uint MeshIndex : register(b0);