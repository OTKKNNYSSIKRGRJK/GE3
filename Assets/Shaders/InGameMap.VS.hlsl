struct MAPMETADATA {
	uint Width;
	uint Height;
};

StructuredBuffer<int> Map : register(t0);
ConstantBuffer<MAPMETADATA> MapMetadata : register(b1);
StructuredBuffer<uint> Indices_Active : register(t0, space2);

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

//static const float SeaElevation = 0.5f;

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

//#define LOG2_WIDTH 9
//#define WIDTHMASK ((1 << LOG2_WIDTH) - 1)

//uint2 UV(uint instID_) {
//	return uint2(instID_ & WIDTHMASK, instID_ >> LOG2_WIDTH);
//}

//uint ArrayIndex(in uint2 uv_) {
//	return (uv_.y << LOG2_WIDTH) + uv_.x;
//}

//void SetVSOutput(inout VSOutput output_, in VSInput input_, uint instID_) {
//	uint2 uv = UV(instID_);
//	int xOffset = input_.LocalPosition.x > 0 ? 1 : 0;
//	int yOffset = input_.LocalPosition.y > 0 ? 1 : 0;
//	uint2 uv2 = uint2((uv.x + xOffset) & WIDTHMASK, (uv.y + yOffset) & WIDTHMASK);
//	MAPSURFLET surflet = MapSurflet[ArrayIndex(uv2)];
//	CLIMATE climate = Climate[ArrayIndex(uv2)];
	
//	output_.Position.xzyw = input_.LocalPosition;
//	output_.Position.xz += uv * 1.0f;
//	output_.Position.xz *= 0.02f;
//	output_.Position.y = surflet.Elevation;
//	output_.Position = mul(output_.Position, VP.Mat);
	
//	output_.Normal = normalize(surflet.Normal);
//	output_.IsWater = surflet.Elevation <= SeaElevation;
//	output_.TexCoord =
//		output_.IsWater ?
//		float2((SeaElevation - surflet.Elevation) / SeaElevation, climate.Temperature) :
//		float2(climate.Precipitation, climate.Temperature);
	
//	output_.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
//}

float3 BlockPosition(int block_) {
	uint x = block_ % MapMetadata.Width;
	uint y = block_ / MapMetadata.Width;
	return float3(x * 2.0f, y * 2.0f, 0.0f);
}

static float v[6] = { 0.1f, 0.2f, 0.0f, 0.1f, 0.3f, 0.2f };

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID, uint vertID_ : SV_VertexID) {
	VSOutput output;
	
	//int block = Map[Indices_Active[instID_]];
	
	output.Position = MeshPositions[input_.IDX_POSITION];
	//output.Position = float4(float(Map[instID_]), 1.0f, 1.0f, 1.0f);
	output.Position.xyz += BlockPosition(Indices_Active[instID_]);
	output.Position = mul(output.Position, VP.Mat);
	//output.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	float3 color0 = float3(MeshTexCoords[input_.IDX_TEXCOORD], 1.0f);
	float3 color1 = MeshNormals[input_.IDX_NORMAL] * 0.5f + 0.5f;
	output.Color = float4((color1) * 0.125f, 1.0f);
	if (Map[Indices_Active[instID_]] == 2) {
		output.Color.rgb *= 2.0f;
	}
	
	output.Color.r += v[vertID_ % 6];
	
	//SetVSOutput(output, input_, instID_);
	
	return output;
}