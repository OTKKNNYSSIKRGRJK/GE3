struct RENDER_DATA {
	float4x4 Transform;
	uint ElementType;
	float Opacity;
};

StructuredBuffer<RENDER_DATA> BulletPool : register(t0);
StructuredBuffer<uint> Indices_Active : register(t1);

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
};

struct ViewProjection {
	float4x4 Mat;
};
ConstantBuffer<ViewProjection> VP : register(b0, space1);

static float3 ElementColor[6] = {
	float3(0.3f, 0.7f, 0.2f),
	float3(0.8f, 0.1f, 0.1f),
	float3(0.6f, 0.3f, 0.5f),
	float3(0.9f, 0.9f, 0.3f),
	float3(0.1f, 0.2f, 0.8f),
	float3(0.8f, 0.8f, 0.8f),
};

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID, uint vertID_ : SV_VertexID) {
	VSOutput output;
	
	output.Position = MeshPositions[input_.IDX_POSITION];
	RENDER_DATA data = BulletPool[Indices_Active[instID_]];
	output.Position = mul(output.Position, data.Transform);
	output.Position = mul(output.Position, VP.Mat);
	//output.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	float3 color0 = float3(MeshTexCoords[input_.IDX_TEXCOORD], 1.0f);
	output.Color = float4(color0 * ElementColor[data.ElementType], 0.25f * data.Opacity);
	
	//SetVSOutput(output, input_, instID_);
	
	return output;
}