#include"Common.hlsli"

cbuffer Scene : register(b0) {
	float4x4 WorldToNDC;
};

cbuffer Model : register(b1) {
	float4x4 LocalToWorld;
	float4x4 Transpose_WorldToLocal;
}

CG3::VSOutput main(CG3::VSInput input_) {
	CG3::VSOutput output;
	
	output.Position = mul(mul(float4(input_.LocalPos, 1.0f), LocalToWorld), WorldToNDC);
	output.TexCoord = input_.TexCoord;
	output.TexID = input_.TexID;
	output.Normal = normalize(mul(input_.Normal, (float3x3) Transpose_WorldToLocal));
	
	return output;
}