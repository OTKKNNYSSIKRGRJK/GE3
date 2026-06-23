struct VSInput {
	float4 Position : POSITION0;
};

struct VSOutput {
	float4 SVPos : SV_Position;
	uint InstanceID : INSTID0;
};

StructuredBuffer<float4x4> SRV_Matrices_World : register(t0);
cbuffer CBV_Scene : register(b0) {
	float4x4 WorldToNDC;
}

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID) {
	VSOutput output;
	
	float4 worldPos = mul(input_.Position, SRV_Matrices_World[instID_]);
	output.SVPos = mul(worldPos, WorldToNDC);
	output.InstanceID = instID_;
	
	return output;
}

VSOutput Fullscreen(VSInput input_, uint instID_ : SV_InstanceID) {
	VSOutput output;
	
	output.SVPos = input_.Position;
	output.InstanceID = instID_;
	
	return output;
}