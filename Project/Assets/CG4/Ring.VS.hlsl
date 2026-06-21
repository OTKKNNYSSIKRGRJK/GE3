struct VSInput {
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

cbuffer Constants : register(b0) {
	float4x4 WorldToProjective;
	float4x4 ViewToWorld;
}

StructuredBuffer<float4x4> Worlds : register(t0, space1);

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID) {
	VSOutput output;
	//output.Position = float4(mul(input_.Position.xyz, (float3x3) ViewToWorld), 1.0f);
	output.Position = mul(input_.Position, Worlds[instID_]);
	output.Position = mul(output.Position, WorldToProjective);
	output.TexCoord = input_.TexCoord;
	output.TexCoord.x += sin(output.Position.x * 1.1f + output.Position.y * 1.2f + output.Position.z) * 1.5f;
	output.TexCoord.y += 0.05f;
	return output;
}