struct Mat4 {
	float4x4 Mat;
};

struct QuadUVs {
	float2 UV[4];
};

StructuredBuffer<float4x4> SRV_Worlds : register(t0, space16);
ConstantBuffer<Mat4> CBV_VP : register(b0, space16);

StructuredBuffer<QuadUVs> SRV_UVs : register(t0, space3);

struct VSInput {
	float4 Position : POSITION0;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	uint InstanceID : INSTID0;
};

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID, uint vertID_ : SV_VertexID) {
	VSOutput output;
	
	output.Position = mul(mul(input_.Position, SRV_Worlds[instID_]), CBV_VP.Mat);
	output.TexCoord = SRV_UVs[instID_].UV[vertID_];
	output.InstanceID = instID_;
	
	return output;
}