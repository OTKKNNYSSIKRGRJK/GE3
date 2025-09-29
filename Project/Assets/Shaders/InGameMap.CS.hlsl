struct EnvironmentVariables {
	float Time;
	float AccelFieldCoefs[9];
};

struct MapBlock {
	float4 Position;
	uint TextureID;
};

struct MAPMETADATA {
	uint Width;
	uint Height;
};

StructuredBuffer<int> MapBlockData : register(t0);
ConstantBuffer<MAPMETADATA> MapMetadata : register(b1);

AppendStructuredBuffer<uint> In_Indices_Active : register(u2);
//RWStructuredBuffer<MapBlock> MapBlockPool : register(u3);
//AppendStructuredBuffer<uint> In_Indices_Inactive : register(u4);
//ConsumeStructuredBuffer<uint> Out_Indices_Inacitve : register(u5);

//ConstantBuffer<EnvironmentVariables> EnvVars : register(b0);

#define NUM_THREAD_X 256U

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

#define BITMASK_BLOCK_TYPE ((1 << 20) - 1)

bool CheckCulling(uint blockID_) {
	return (MapBlockData[blockID_] & BITMASK_BLOCK_TYPE);
}

void UpdateRenderList(uint blockID_) {
	if (CheckCulling(blockID_)) {
		In_Indices_Active.Append(blockID_);
	}
}

[numthreads(NUM_THREAD_X, 1U, 1U)]
void Update(uint3 dtid_ : SV_DispatchThreadID) {
	uint idx = dtid_.x;
	
	UpdateRenderList(idx);
}