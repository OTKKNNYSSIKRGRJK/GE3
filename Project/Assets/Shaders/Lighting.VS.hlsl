struct VSInput {
	float4 Position : POSITION0;
};

struct VSOutput {
	float4 Position : SV_Position;
	uint LightID : ID0;
};

struct Mat4 {
	float4x4 Mat;
};

StructuredBuffer<float4x4> SRV_Arr_Matrix_World : register(t0);
ConstantBuffer<Mat4> CBV_Matrix_WorldToNDC : register(b0);

StructuredBuffer<uint> SRV_Arr_Index : register(t0, space2);

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID) {
	uint id = SRV_Arr_Index[instID_];
	
	VSOutput output;
	output.Position = mul(
		mul(input_.Position, SRV_Arr_Matrix_World[id]),
		CBV_Matrix_WorldToNDC.Mat
	);
	output.LightID = id;
	
	return output;
}