StructuredBuffer<float4> MeshPositions : register(t0, space3);
StructuredBuffer<float2> MeshTexCoords : register(t1, space3);
StructuredBuffer<float3> MeshNormals : register(t2, space3);

struct VSInput {
	uint3 Data : DATA0;
};

#define IDX_POSITION Data.x
#define IDX_TEXCOORD Data.y
#define IDX_NORMAL Data.z

struct VSOutput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct ViewProjection {
	float4x4 Mat;
};
ConstantBuffer<ViewProjection> VP : register(b0, space1);

struct RENDER_DATA {
	float4x4 Transform;
};
ConstantBuffer<RENDER_DATA> RenderData : register(b0);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID, uint vertID_ : SV_VertexID) {
	VSOutput output;
	
	//int block = Map[Indices_Active[instID_]];
	
	output.Position = MeshPositions[input_.IDX_POSITION];
	output.Position = mul(output.Position, RenderData.Transform);
	output.Position = mul(output.Position, VP.Mat);
	//output.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	float2 color = MeshTexCoords[input_.IDX_TEXCOORD] * 0.8f + 0.3f;
	output.Color = float4(color, 0.8f, 1.0f);
	output.TexCoord = MeshTexCoords[input_.IDX_TEXCOORD];
	
	//SetVSOutput(output, input_, instID_);
	
	return output;
}