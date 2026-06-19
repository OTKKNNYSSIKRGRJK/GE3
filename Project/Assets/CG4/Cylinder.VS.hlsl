struct VSInput {
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
	float3 LocalNormal : NORMAL1;
	float WorldZ : POS1;
};

cbuffer Constants : register(b0) {
	float4x4 WorldToProjective;
	float Time;
}

StructuredBuffer<float4x4> Worlds : register(t0, space1);

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID) {
	VSOutput output;
	output.Position = mul(input_.Position, Worlds[instID_]);
	output.WorldZ = output.Position.z;
	output.Position = mul(output.Position, WorldToProjective);
	output.TexCoord = input_.TexCoord;
	output.TexCoord.x += Time * 0.1f + output.WorldZ * 0.1f;
	output.TexCoord.y += cos(Time * 7.0f + input_.Normal.x * 5.0f) * 0.1f + 0.1f;
	output.Normal = mul(input_.Normal, (float3x3) WorldToProjective);
	output.LocalNormal = input_.Normal;
	return output;
}