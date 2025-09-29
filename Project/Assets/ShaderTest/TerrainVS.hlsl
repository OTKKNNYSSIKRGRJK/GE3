struct MAPSURFLET {
	float Elevation;
	float3 Normal;
};

struct CLIMATE {
	float Temperature;
	float Precipitation;
};

StructuredBuffer<MAPSURFLET> MapSurflet : register(t0);
StructuredBuffer<CLIMATE> Climate : register(t1);

struct VSInput {
	float4 LocalPosition : POSITION0;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
	uint IsWater : FLAG0;
};

struct ViewProjection {
	float4x4 Mat;
};
ConstantBuffer<ViewProjection> VP : register(b0);

static const float SeaElevation = 0.5f;

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

#define LOG2_WIDTH 9
#define WIDTHMASK ((1 << LOG2_WIDTH) - 1)

uint2 UV(uint instID_) {
	return uint2(instID_ & WIDTHMASK, instID_ >> LOG2_WIDTH);
}

uint ArrayIndex(in uint2 uv_) {
	return (uv_.y << LOG2_WIDTH) + uv_.x;
}

void SetVSOutput(inout VSOutput output_, in VSInput input_, uint instID_) {
	uint2 uv = UV(instID_);
	int xOffset = input_.LocalPosition.x > 0 ? 1 : 0;
	int yOffset = input_.LocalPosition.y > 0 ? 1 : 0;
	uint2 uv2 = uint2((uv.x + xOffset) & WIDTHMASK, (uv.y + yOffset) & WIDTHMASK);
	MAPSURFLET surflet = MapSurflet[ArrayIndex(uv2)];
	CLIMATE climate = Climate[ArrayIndex(uv2)];
	
	output_.Position.xzyw = input_.LocalPosition;
	output_.Position.xz += uv * 1.0f;
	output_.Position.xz *= 0.02f;
	output_.Position.y = surflet.Elevation;
	output_.Position = mul(output_.Position, VP.Mat);
	
	output_.Normal = normalize(surflet.Normal);
	output_.IsWater = surflet.Elevation <= SeaElevation;
	output_.TexCoord =
		output_.IsWater ?
		float2((SeaElevation - surflet.Elevation) / SeaElevation, climate.Temperature) :
		float2(climate.Precipitation, climate.Temperature);
	
	output_.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
}

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID) {
	VSOutput output;
	
	SetVSOutput(output, input_, instID_);
	
	return output;
}