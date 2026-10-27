//////	//////	//////	//////	//////	//////
//	GameOverGrade（ゲームオーバーの色抜け）	//
//////	//////	//////	//////	//////	//////
// UI まで描き終えた画面（中間 RT。SRGB なので読むとリニア）を、バックバッファ（SRGB）へフルスクリーンで写す。
// 1 本の PS で：グラデーションマップ（Clip Studio 風）→ ビネット
//
// 色の空間：Clip Studio のグラデーションマップに近い見た目にするため、輝度・ストップの補間・ブレンド・ビネットは
//           すべて sRGB（ガンマ空間）で行い、最後にリニアへ戻して書く（RT が SRGB なのでハードウェアが再度エンコードする）。
//           リニアのまま輝度を取ると暗部が潰れる。
// グラデーションマップ：輝度 = Rec.709 の係数を sRGB 値に掛けたもの
//           → t = pow(saturate((輝度 − BlackPoint) / (WhitePoint − BlackPoint)), 1 / Gamma)
//           → ストップ列（位置の昇順。両端の外側は端の色のまま）から色を引く
//           → GradientAmount で元の絵とブレンド
// ビネット：画面の端ほど VignetteColor へ寄せる。中心からの距離は「角で 1」に正規化する。
//           Roundness 0 = 画面の縦横比に沿った楕円、1 = 真円
// 8bit の暗部が段になるのを避けるため、出力前に ±0.5/255 のノイズを足す。
// VSMain : SV_VertexID から画面全体を覆う三角形を 1 枚生成（頂点バッファなし）

static const uint MaxStops = 8U;

cbuffer Constants : register(b0) {
	float4 Stops[MaxStops];			// xyz = 色（sRGB）、w = 位置（0..1、昇順）

	float GradientAmount;			// 0 = 元の絵のまま、1 = グラデーションマップのみ
	float BlackPoint;
	float WhitePoint;
	float Gamma;					// > 1 で明るい側へ寄る

	uint NumStops;					// 2..MaxStops
	float VignetteAmount;			// 0..1（強さ × 進行）
	float VignetteRadius;			// ここまでは変化なし（角で 1）
	float VignetteSoftness;			// Radius からこの幅で VignetteColor へ

	float3 VignetteColor;			// sRGB
	float VignetteRoundness;

	float Aspect;					// 幅 / 高さ
	float SplitPosition;			// 0..1：この UV.x より左は元の絵を出す（比較用）。負なら無効
	float ViewLuma;					// 1 = レベル補正後の輝度だけを白黒で出す
	float DitherAmount;				// 出力前に足すノイズの幅（sRGB 値）
};

Texture2D<float4> SceneTexture : register(t0);
SamplerState LinearClamp : register(s0);

struct VSOutput {
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
};

VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	// (0,0) (2,0) (0,2) の UV で、画面を覆う大きな三角形
	const float2 uv = float2((vertexID_ << 1U) & 2U, vertexID_ & 2U);

	VSOutput output;
	output.Position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	output.UV = uv;
	return output;
}

//////	//////	//////	//////	//////	//////
//	Helpers									//
//////	//////	//////	//////	//////	//////

float3 SRGBToLinear(float3 c_) {
	c_ = saturate(c_);
	const float3 lo = c_ / 12.92f;
	const float3 hi = pow((c_ + 0.055f) / 1.055f, 2.4f);
	return lerp(lo, hi, step(0.04045f, c_));
}

float3 LinearToSRGB(float3 c_) {
	c_ = saturate(c_);
	const float3 lo = c_ * 12.92f;
	const float3 hi = 1.055f * pow(c_, 1.0f / 2.4f) - 0.055f;
	return lerp(lo, hi, step(0.0031308f, c_));
}

float Hash12(float2 p_) {
	return frac(sin(dot(p_, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// ストップ列から色（sRGB）を引く。位置は昇順であること
float3 EvaluateGradient(float t_) {
	if (t_ <= Stops[0].w) { return Stops[0].rgb; }
	const uint last = max(NumStops, 2U) - 1U;
	[loop]
	for (uint i = 1U; i <= last; ++i) {
		const float p0 = Stops[i - 1U].w;
		const float p1 = Stops[i].w;
		if (t_ <= p1) {
			const float f = saturate((t_ - p0) / max(p1 - p0, 1.0e-5f));
			return lerp(Stops[i - 1U].rgb, Stops[i].rgb, f);
		}
	}
	return Stops[last].rgb;
}

float Vignette(float2 uv_) {
	float2 p = (uv_ - 0.5f) * 2.0f;
	const float ax = lerp(1.0f, Aspect, VignetteRoundness);
	p.x *= ax;
	const float d = length(p) * rsqrt(ax * ax + 1.0f);		// 角で 1
	return smoothstep(VignetteRadius, VignetteRadius + max(VignetteSoftness, 1.0e-4f), d);
}

//////	//////	//////	//////	//////	//////
//	PSMain									//
//////	//////	//////	//////	//////	//////

float4 PSMain(VSOutput input_) : SV_Target {
	const float3 source = LinearToSRGB(SceneTexture.SampleLevel(LinearClamp, input_.UV, 0.0f).rgb);

	const float luma = dot(source, float3(0.2126f, 0.7152f, 0.0722f));
	float t = saturate((luma - BlackPoint) / max(WhitePoint - BlackPoint, 1.0e-4f));
	t = pow(t, 1.0f / max(Gamma, 1.0e-3f));

	float3 color = lerp(source, EvaluateGradient(t), GradientAmount);

	if (ViewLuma > 0.5f) {
		color = t.xxx;
	}
	else {
		color = lerp(color, VignetteColor, saturate(Vignette(input_.UV) * VignetteAmount));
	}

	if (input_.UV.x < SplitPosition) {
		color = source;
	}

	color += (Hash12(input_.Position.xy) - 0.5f) * DitherAmount;
	return float4(SRGBToLinear(color), 1.0f);
}
