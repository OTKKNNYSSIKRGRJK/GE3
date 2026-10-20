//////	//////	//////	//////	//////	//////
//	VsBanner の背景パーティクル（Lumina.VsBanner の ParticleRenderer）	//
//////	//////	//////	//////	//////	//////
// 粒の位置・大きさ・色は CPU で計算済み（RT の中心からの px、y 上）。ここは並べて描くだけ。
// VSMain : SV_InstanceID で StructuredBuffer（ルート SRV）から 1 粒読み、SV_VertexID で向き付きのクアッドを作る
// PSMain : カプセル（流れる向きに伸ばした円）までの距離から、ぼかした点〜縁のはっきりした円盤を作る。加算（α は書かない）

cbuffer Constants : register(b0) {
	float2 InvHalfTarget;	// (2 / RT の幅, 2 / RT の高さ)
	float2 Padding0;
};

struct Particle {
	float2 Center;			// RT の中心からの px（y 上）
	float2 Axis;			// 伸ばす向き（単位ベクトル）
	float HalfLength;		// 伸ばす向きの半分の長さ（px、HalfWidth 以上）
	float HalfWidth;		// 太さの半分（px）
	float Hardness;			// 0 = ぼかした点、1 = 円盤
	float Padding1;
	float3 Color;			// リニア。強さとフェードを掛け済み
	float Padding2;
};

StructuredBuffer<Particle> Particles : register(t0);

struct VSOutput {
	float4 Position : SV_Position;
	float2 Local : TEXCOORD0;							// 粒のローカル座標（px）。x = 伸ばす向き
	nointerpolation float HalfLength : TEXCOORD1;
	nointerpolation float HalfWidth : TEXCOORD2;
	nointerpolation float Hardness : TEXCOORD3;
	nointerpolation float3 Color : TEXCOORD4;
};

VSOutput VSMain(uint vertexID_ : SV_VertexID, uint instanceID_ : SV_InstanceID) {
	static const float2 Corners[6] = {
		float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
		float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f),
	};
	const Particle p = Particles[instanceID_];

	// アンチエイリアスの分だけ 1 px 広げる
	const float2 local = Corners[vertexID_] * float2(p.HalfLength + 1.0f, p.HalfWidth + 1.0f);
	const float2 normal = float2(-p.Axis.y, p.Axis.x);
	const float2 position = p.Center + p.Axis * local.x + normal * local.y;

	VSOutput output;
	output.Position = float4(position * InvHalfTarget, 0.0f, 1.0f);
	output.Local = local;
	output.HalfLength = p.HalfLength;
	output.HalfWidth = p.HalfWidth;
	output.Hardness = p.Hardness;
	output.Color = p.Color;
	return output;
}

float4 PSMain(VSOutput input_) : SV_Target {
	// カプセルの芯（長さ HalfLength − HalfWidth の線分）からの距離を、太さで割って 0（芯）〜 1（縁）にする
	const float2 q = float2(max(abs(input_.Local.x) - (input_.HalfLength - input_.HalfWidth), 0.0f), input_.Local.y);
	const float r = length(q) / max(input_.HalfWidth, 1.0e-4f);

	const float soft = pow(saturate(1.0f - r * r), 2.0f);					// ぼかした点
	const float disc = saturate((1.0f - r) / max(fwidth(r), 1.0e-4f));		// 縁のはっきりした円盤（1 px でアンチエイリアス）
	const float shape = lerp(soft, disc, saturate(input_.Hardness));

	return float4(input_.Color * shape, 0.0f);
}
