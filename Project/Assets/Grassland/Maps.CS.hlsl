#include "Noise.hlsli"
#include "Color.hlsli"

cbuffer System : register(b0) {
	float Time;
};

cbuffer Scene : register(b1) {
	float4x4 WorldToNDC;
	float3 PlayerWorldPos;
	float PlayerRadius;
	float3 EnemyWorldPos;
	float EnemyRadius;
};

#define SLOT_UAV_MAP_BEND register(u0)
#define SLOT_UAV_MAP_TRAMPLING register(u1)
#define SLOT_UAV_MAP_NOISE register(u2)
#define SLOT_UAV_MAP_TINT register(u3)

RWTexture2D<float4> UAV_Map_Bend : SLOT_UAV_MAP_BEND;
RWTexture2D<float4> UAV_Map_Trampling : SLOT_UAV_MAP_TRAMPLING;
RWTexture2D<float4> UAV_Map_Noise : SLOT_UAV_MAP_NOISE;
RWTexture2D<float4> UAV_Map_Tint : SLOT_UAV_MAP_TINT;

Texture2D<float4> SRV_Map_Noise : register(t2);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

static const float TintNoisePersistence = 0.75f;
static const PerlinNoise::Args TintNoiseArgs = {
	0.25f,
	float3(0.0f, 0.0f, 0.0f),
	TintNoisePersistence,
	2U,
	1.0f / (
		1.0f +
		TintNoisePersistence
	)
};

[numthreads(1U, 1U, 1U)]
void GenerateTintMap(uint3 dtid_ : SV_DispatchThreadID) {
	float3 color0 = Color::HSVToRGB(
		PerlinNoise::Generate(dtid_.zxy * 0.5f, TintNoiseArgs) * 120.0f,
		0.5f + PerlinNoise::Generate(dtid_.yzx * 0.05f, TintNoiseArgs) * 0.25f,
		0.8f
	);
	float3 color1 = Color::HSVToRGB(
		PerlinNoise::Generate(dtid_.xyz * 0.05f, TintNoiseArgs) * 120.0f,
		0.5f,
		0.95f
	);
	UAV_Map_Tint[dtid_.xy] = float4(color0 * color1, 1.0f);
}

//////	//////	//////	//////	//////	//////

static const float NoisePersistence = 0.75f;
static const PerlinNoise::Args NoiseArgs = {
	0.5f,
	float3(0.5f, 0.6f, 0.7f),
	NoisePersistence,
	4U,
	1.0f / (
		1.0f +
		NoisePersistence +
		NoisePersistence * NoisePersistence +
		NoisePersistence * NoisePersistence * NoisePersistence
	)
};

[numthreads(1U, 1U, 1U)]
void GenerateNoiseMap(uint3 dtid_ : SV_DispatchThreadID) {
	UAV_Map_Noise[dtid_.xy] = float4(
		PerlinNoise::Generate(dtid_.xyz * 1.0f, NoiseArgs),
		PerlinNoise::Generate(dtid_.yzx * 2.0f, NoiseArgs),
		PerlinNoise::Generate(dtid_.zxy * 3.0f, NoiseArgs),
		PerlinNoise::Generate(dtid_.xzy * 5.0f, NoiseArgs)
	);
}

//////	//////	//////	//////	//////	//////

static const float Inv_PlayerRadius = 1.0f / PlayerRadius;
static const float Inv_EnemyRadius = 1.0f / EnemyRadius;

float2 DispatchThreadIDToWorldPosXZ(in uint3 dtid_) {
	return (float2(dtid_.xy) + float2(-131.0f, -131.0f)) * float2(0.3f, 0.3f);
}

void GenerateTramplingMap_Impl(uint3 dtid_, in float3 worldPos_, float radius_, float inv_Radius_) {
	//const float2 worldPosXZ = DispatchThreadIDToWorldPosXZ(dtid_);
	//const float2 r = worldPosXZ - worldPos_.xz;
	//const float d2 = radius_ * radius_ - dot(r, r);
	//const float t = dot(r, r) * (inv_Radius_ * inv_Radius_);
	
	//UAV_Map_Trampling[dtid_.xy].xyz =
	//	d2 > 0.0f ?
	//	lerp(
	//		normalize(float3(r.x, -radius_, r.y)),
	//		UAV_Map_Trampling[dtid_.xy].xyz,
	//		t
	//	) :
	//	UAV_Map_Trampling[dtid_.xy].xyz * 0.995f + float3(0.0f, 0.005f, 0.0f);
	
	//UAV_Map_Trampling[dtid_.xy].w =
	//	d2 > 0.0f ?
	//	0.01f :
	//	saturate(UAV_Map_Trampling[dtid_.xy].w * 1.02f);
	
	const float2 worldPosXZ = DispatchThreadIDToWorldPosXZ(dtid_);
	const float2 r = worldPosXZ - worldPos_.xz;
	const float d2 = radius_ * radius_ - dot(r, r);
	const float t = dot(r, r) * (inv_Radius_ * inv_Radius_);
	
	float4 pn_DITD = SRV_Map_Noise.Load(dtid_);
	
	float2 sampleOffset = float2(WaveGetLaneIndex() & 1, (WaveGetLaneIndex() >> 1) & 1);
	float4 pn_Pos = SRV_Map_Noise.Load(
		int3(
			(worldPos_.xz + sampleOffset) * 0.1f,
			0.0f
		) & 0xFF
	);
	const float4 pn_Pos0 = WaveReadLaneAt(pn_Pos, 0U);
	const float4 pn_Pos1 = WaveReadLaneAt(pn_Pos, 1U);
	const float4 pn_Pos2 = WaveReadLaneAt(pn_Pos, 2U);
	const float4 pn_Pos3 = WaveReadLaneAt(pn_Pos, 3U);
	
	const float4 pn_Pos_Lerp01 = lerp(pn_Pos0, pn_Pos1, frac(worldPos_.x));
	const float4 pn_Pos_Lerp23 = lerp(pn_Pos2, pn_Pos3, frac(worldPos_.x));
	const float4 pn_Pos_Lerp = lerp(pn_Pos_Lerp01, pn_Pos_Lerp23, frac(worldPos_.z));
	
	UAV_Map_Trampling[dtid_.xy].xyz =
		d2 > 0.0f ?
		lerp(
			normalize(
				float3(r.x, 0.0f, r.y) +
				(pn_DITD.zwx - 0.5f) * float3(200.0f, 25.0f, 200.0f) * (1.0f - t) +
				(pn_Pos_Lerp.wxy - 0.5f) * float3(25.0f, 5.0f, 25.0f)
			) + float3(0.0f, -1.2f, 0.0f),
			UAV_Map_Trampling[dtid_.xy].xyz,
			t
		) :
		UAV_Map_Trampling[dtid_.xy].xyz * 0.999f + float3(0.0f, 0.001f, 0.0f);
	
	UAV_Map_Trampling[dtid_.xy].w =
		d2 > 0.0f ?
		0.5f * t :
		saturate(UAV_Map_Trampling[dtid_.xy].w * 1.01f);
}

[numthreads(2U, 2U, 1U)]
void GenerateTramplingMap(uint3 dtid_ : SV_DispatchThreadID) {
	GenerateTramplingMap_Impl(dtid_, PlayerWorldPos, PlayerRadius, Inv_PlayerRadius);
	GenerateTramplingMap_Impl(dtid_, EnemyWorldPos, EnemyRadius, Inv_EnemyRadius);
}

//////	//////	//////	//////	//////	//////

float3 Wavelet(in uint3 dtid_) {
	float4 pn = SRV_Map_Noise.Load(dtid_);
	return
		normalize(float3(1.0f + (pn.z - 0.5f) * 0.5f, (pn.x - 0.5f) * 0.01f, (pn.w - 0.5f) * 0.25f)) *
		sin(Time * 0.025f + pn.wyz * 2.5f) * 0.25f * (1.0f + pn.ywx);
}

float3 Gerstner(in uint3 dtid_) {
	float4 pn = SRV_Map_Noise.Load(dtid_);
	return float3(
		cos(dtid_.x * 0.02f + pn.x * 10.0f + Time * 0.0125f) * 0.5f +
		cos(dtid_.x * 0.05f + pn.y * 20.0f + Time * 0.0125f) * 0.1f,
		sin(dtid_.x * 0.02f + pn.w * 10.0f + Time * 0.0125f) * 0.05f +
		sin(dtid_.x * 0.05f + pn.z * 20.0f + Time * 0.0125f) * 0.01f,
		0.0f
	);
}

[numthreads(1U, 1U, 1U)]
void GenerateBendMap(uint3 dtid_ : SV_DispatchThreadID) {
	//float4 args_Trampling = UAV_Map_Trampling.Load(dtid_);
	UAV_Map_Bend[dtid_.xy] = float4(
		Wavelet(dtid_) + Gerstner(dtid_),
		0.75f
	);
}

//////	//////	//////	//////	//////	//////

[numthreads(1U, 1U, 1U)]
void InitMaps(uint3 dtid_ : SV_DispatchThreadID) {
	UAV_Map_Bend[dtid_.xy] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	UAV_Map_Trampling[dtid_.xy] = float4(0.0f, 1.0f, 0.0f, 1.0f);
}