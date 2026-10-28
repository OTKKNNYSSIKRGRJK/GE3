struct VSOutput {
	float4 Pos : SV_Position;
	float3 WorldPos : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
	float3 LocalPos : POSITION1;
};

struct PSOutput {
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 Factors0 : SV_TARGET2;
	//float4 Specular : SV_TARGET3;
};

Texture2D<float4> Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

struct MATERIAL {
	float4 Color;
	uint ID_DiffuseMap;
	uint ID_SpecularMap;
	uint ID_NormalMap;
};
#define SPACE_MATERIAL space3
ConstantBuffer<MATERIAL> Material : register(b0, SPACE_MATERIAL);

TextureCube<float4> SRV_EnvironmentMap : register(t0, space17);
// ルート定数（12 個）。C++ 側は Game.GameOverShatter の SkinnedMeshConstants（並びを合わせること）
cbuffer Parameter_Space17Slot0 : register(b0, space17) {
	float3 WorldPos_Camera;
	float DissolveThreshold;	// これ以下のノイズ値は消える。-1 未満ならディゾルブなし（ボスはこれ）
	float NoiseScale;			// バインドポーズの位置に掛ける（体の大きさに対するノイズの細かさ）
	float NoiseContrast;
	float EdgeWidth;			// 消える縁（ノイズ値の幅）
	float EdgeBoost;
	float3 EdgeColor;
	uint NoiseSeed;
}

//////	//////	//////	//////	//////	//////
//	ディゾルブ用のノイズ					//
//////	//////	//////	//////	//////	//////
// 3 次元バリューノイズ（3 オクターブ）。C++ 側（Game.GameOverShatter.cpp の DissolveNoise）と同じ式にしてある：
// 粒子を「いま消えている場所」から出すために、CPU でも同じ値を計算するため。式を変えるときは両方そろえること

uint Hash(uint x) {
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

float Lattice(int3 p, uint seed) {
	uint h = Hash(asuint(p.x) * 73856093U ^ asuint(p.y) * 19349663U ^ asuint(p.z) * 83492791U ^ seed);
	return float(h >> 8) * (1.0f / 16777216.0f);
}

float ValueNoise(float3 p, uint seed) {
	float3 i = floor(p);
	float3 f = p - i;
	float3 u = f * f * (3.0f - 2.0f * f);
	int3 c = int3(i);
	float v000 = Lattice(c + int3(0, 0, 0), seed);
	float v100 = Lattice(c + int3(1, 0, 0), seed);
	float v010 = Lattice(c + int3(0, 1, 0), seed);
	float v110 = Lattice(c + int3(1, 1, 0), seed);
	float v001 = Lattice(c + int3(0, 0, 1), seed);
	float v101 = Lattice(c + int3(1, 0, 1), seed);
	float v011 = Lattice(c + int3(0, 1, 1), seed);
	float v111 = Lattice(c + int3(1, 1, 1), seed);
	float x00 = v000 + (v100 - v000) * u.x;
	float x10 = v010 + (v110 - v010) * u.x;
	float x01 = v001 + (v101 - v001) * u.x;
	float x11 = v011 + (v111 - v011) * u.x;
	float y0 = x00 + (x10 - x00) * u.y;
	float y1 = x01 + (x11 - x01) * u.y;
	return y0 + (y1 - y0) * u.z;
}

float DissolveNoise(float3 p, uint seed, float contrast) {
	float n = ValueNoise(p, seed) * 0.5f + ValueNoise(p * 2.03f, seed + 1U) * 0.3f + ValueNoise(p * 4.11f, seed + 2U) * 0.2f;
	return saturate((n - 0.5f) * contrast + 0.5f);
}

float4 CalcEnv(
	in float3 worldPos_,
	in float3 norm_
) {
	const float3 dir_EyeToTarget = normalize(worldPos_ - WorldPos_Camera.xyz);
	
	const float3 refl = reflect(dir_EyeToTarget, norm_);
	float4 envColor = SRV_EnvironmentMap.Sample(Sampler, refl);
	envColor.rgb *= float3(0.02f, 0.04f, 0.05f);
	
	return envColor;
}

PSOutput main(VSOutput input_) {
	PSOutput output;
	
	// ゲームオーバーのディゾルブ：ノイズ値が閾値以下の所は描かない。閾値のすぐ上は縁（光る）
	float dissolveEdge = 0.0f;
	[branch]
	if (DissolveThreshold > -1.0f) {
		float dissolveValue = DissolveNoise(input_.LocalPos * NoiseScale, NoiseSeed, NoiseContrast);
		float d = dissolveValue - DissolveThreshold;
		clip(d);
		dissolveEdge = 1.0f - saturate(d / max(EdgeWidth, 1.0e-4f));
	}
	
	const float4 diffuseColor = Textures[Material.ID_DiffuseMap].Sample(Sampler, input_.TexCoord);
	output.Diffuse = diffuseColor * Material.Color;
	
	const float4 envColor = CalcEnv(input_.WorldPos.xyz, input_.Normal);
	output.Diffuse = output.Diffuse * 0.1f + envColor * 0.9f;
	// 縁は点光源で照らされる前のアルベドなので、光り方は周りのライト次第（粒子の火の粉が主役）
	output.Diffuse.rgb = lerp(output.Diffuse.rgb, EdgeColor * EdgeBoost, dissolveEdge * dissolveEdge);
	
	const float3 normal = float3(0.0f, 0.0f, 1.0f);
	output.Normal = float4(normal * 0.5f + 0.5f, 1.0f);
	// * Bleeding
	output.Factors0.r = abs(normal.z);
	// * Edge Density
	output.Factors0.g = abs(cos(input_.Pos * 0.01f));
	// * Depth
	//output.Factors0.b = input_.Pos.z / input_.Pos.w;
	
	return output;
}