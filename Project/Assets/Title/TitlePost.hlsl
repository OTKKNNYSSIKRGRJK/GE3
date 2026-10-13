//////	//////	//////	//////	//////	//////
//	TitlePost（Title のポスト処理）			//
//////	//////	//////	//////	//////	//////
// Title 用 Canvas（SRGB。読むとリニア）を、バックバッファ（SRGB）へフルスクリーンで写す。
// 1 本の PS で：割れ目（TitleCrack.hlsli）＋ ズームブラー（Center へ向かって）＋ 色収差（Center から離れるほど RGB をずらす）
//              ＋ 簡易ブルーム（黄金角スパイラル。ピクセルごとに回転をずらす）
// シーンは割れ目を反映した読み取り越しに読む。ズームブラー・色収差は SampleComposite（割れ目の光・斬撃線を含む）、
// ブルームは SampleCompositeForBloom（割れ目の光は 1 まで、斬撃線・ふちのにじみは含めない）。
// 割れ目の細くて明るい光をまばらなタップで拾うと、当たり外れでノイズになるため。
// 割れ目まわりの光のにじみは、TitleCrack.hlsli の斬撃線・ふちのにじみ（距離から計算）が受け持つ。
// 効果がすべて 0 のときはそのまま写す（Load 1 回）。
// VSMain : SV_VertexID から画面全体を覆う三角形を 1 枚生成（頂点バッファなし）

#include"TitleCrack.hlsli"

cbuffer Constants : register(b0) {
	float2 Center;				// ズームブラーの中心（画面 UV、左上が 0,0）
	float ZoomBlur;				// 中心へ向かってぼかす長さ（中心までの距離に対する割合）
	float ZoomTaps;				// 1..MaxZoomTaps

	float Chromatic;			// Center から離れた所での RGB のずれ（UV）
	float BloomIntensity;
	float BloomRadiusPx;
	float BloomThreshold;		// これより明るい分（リニア）だけにじませる

	float BloomTaps;			// 1..MaxBloomTaps
	float Aspect;				// 幅 / 高さ
	float2 Resolution;			// px

	// 割れ目（CrackParams と同じ意味。TitleCrack.hlsli 参照）
	float2 CrackPoint;
	float CrackAngle;
	float CrackGap;

	float CrackRotate;
	float CrackGlowWidth;
	float CrackGlowIntensity;
	float CrackLightIntensity;

	float3 CrackCoreColor;
	float CrackLineWidth;

	float3 CrackEdgeColor;
	float CrackLineIntensity;

	float CrackLineReveal;
	float CrackEnabled;			// 0 = 割れ目なし
	float2 Padding0;
};

Texture2D<float4> SceneTexture : register(t0);
SamplerState LinearClamp : register(s0);

static const uint MaxZoomTaps = 32U;
static const uint MaxBloomTaps = 32U;
static const float TwoPi = 6.28318530718f;
static const float GoldenAngle = 2.39996323f;

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

float Hash12(float2 p_) {
	return frac(sin(dot(p_, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// Interleaved Gradient Noise（Jimenez 2014）。白色ノイズより偏りなく散らばるので、少ないタップのずらしに向く
float InterleavedGradientNoise(float2 pixel_) {
	return frac(52.9829189f * frac(dot(pixel_, float2(0.06711056f, 0.00583715f))));
}

CrackParams MakeCrackParams() {
	CrackParams p;
	p.Point = CrackPoint;
	p.Angle = CrackAngle;
	p.Gap = CrackGap;
	p.Rotate = CrackRotate;
	p.Aspect = Aspect;
	p.CoreColor = CrackCoreColor;
	p.EdgeColor = CrackEdgeColor;
	p.LightIntensity = CrackLightIntensity;
	p.GlowWidth = CrackGlowWidth;
	p.GlowIntensity = CrackGlowIntensity;
	p.LineWidth = CrackLineWidth;
	p.LineIntensity = CrackLineIntensity;
	p.LineReveal = CrackLineReveal;
	return p;
}

// シーンを読む（割れ目があれば、ずらした破片＋割れ目の光＋斬撃線）
float3 SampleComposite(float2 uv_) {
	if (CrackEnabled <= 0.0f) {
		return SceneTexture.SampleLevel(LinearClamp, uv_, 0.0f).rgb;
	}
	const CrackSample crack = EvaluateCrack(uv_, MakeCrackParams());
	const float3 base = crack.IsLight
		? crack.Light
		: SceneTexture.SampleLevel(LinearClamp, crack.SourceUV, 0.0f).rgb;
	return base + crack.Add;
}

// ブルーム用：割れ目の光は 1 までに抑え、斬撃線・ふちのにじみ（crack.Add）は含めない
float3 SampleCompositeForBloom(float2 uv_) {
	if (CrackEnabled <= 0.0f) {
		return SceneTexture.SampleLevel(LinearClamp, uv_, 0.0f).rgb;
	}
	const CrackSample crack = EvaluateCrack(uv_, MakeCrackParams());
	return crack.IsLight
		? min(crack.Light, 1.0f)
		: SceneTexture.SampleLevel(LinearClamp, crack.SourceUV, 0.0f).rgb;
}

// 色収差：R を外側、B を内側へずらす（G はそのまま）
float3 SampleChromatic(float2 uv_, float2 offset_) {
	if (all(offset_ == 0.0f)) { return SampleComposite(uv_); }
	return float3(
		SampleComposite(uv_ + offset_).r,
		SampleComposite(uv_).g,
		SampleComposite(uv_ - offset_).b
	);
}

//////	//////	//////	//////	//////	//////
//	Pixel shader							//
//////	//////	//////	//////	//////	//////

float4 PSMain(VSOutput input_) : SV_Target {
	// * 効果がすべて 0 なら、そのまま写す（通常時のコストを抑える）
	if (ZoomBlur <= 0.0f && Chromatic <= 0.0f && BloomIntensity <= 0.0f && CrackEnabled <= 0.0f) {
		return float4(SceneTexture.Load(int3(input_.Position.xy, 0)).rgb, 1.0f);
	}

	const float2 uv = input_.UV;

	// * 色収差：効果の中心（Center。武器の先端 → 命中後は割れ目の基準点）から外向きに、離れるほど大きく（2 乗）
	const float2 fromCenter = uv - Center;
	const float distanceFromCenter = length(fromCenter * float2(Aspect, 1.0f));	// 画面の高さ = 1
	const float edge = saturate(distanceFromCenter / (0.5f * length(float2(Aspect, 1.0f))));
	const float2 direction = fromCenter / max(length(fromCenter), 1e-5f);
	const float2 chromaticOffset = direction * max(Chromatic, 0.0f) * edge * edge;

	// * ズームブラー：Center へ向かって taps 回。ピクセルごとに開始位置をずらして縞を目立たなくする
	const uint zoomTaps = (ZoomBlur > 0.0f) ? clamp((uint)ZoomTaps, 1U, MaxZoomTaps) : 1U;
	const float2 toCenter = Center - uv;
	const float jitter = (zoomTaps > 1U) ? Hash12(input_.Position.xy) : 0.0f;
	float3 color = float3(0.0f, 0.0f, 0.0f);
	[loop]
	for (uint i = 0U; i < zoomTaps; ++i) {
		const float s = ((float)i + jitter) / (float)zoomTaps * max(ZoomBlur, 0.0f);
		color += SampleChromatic(uv + toCenter * s, chromaticOffset);
	}
	color /= (float)zoomTaps;

	// * 簡易ブルーム：しきい値を超えた分を、黄金角スパイラルのディスクで集める（外側ほど軽い重み）
	//   タップが少ないので、ピクセルごとにスパイラルを回して縞を細かい模様に変える（IGN で偏りなく）
	if (BloomIntensity > 0.0f) {
		const uint bloomTaps = clamp((uint)BloomTaps, 1U, MaxBloomTaps);
		const float rotation = InterleavedGradientNoise(input_.Position.xy) * TwoPi;
		const float2 radiusUV = max(BloomRadiusPx, 0.0f) / Resolution;

		float3 bloom = float3(0.0f, 0.0f, 0.0f);
		float weightSum = 0.0f;
		[loop]
		for (uint i = 0U; i < bloomTaps; ++i) {
			const float r = sqrt(((float)i + 0.5f) / (float)bloomTaps);
			const float angle = (float)i * GoldenAngle + rotation;
			const float2 offset = float2(cos(angle), sin(angle)) * r * radiusUV;
			const float3 bright = max(SampleCompositeForBloom(uv + offset) - BloomThreshold, 0.0f);
			const float weight = exp(-3.0f * r * r);
			bloom += bright * weight;
			weightSum += weight;
		}
		color += bloom / max(weightSum, 1e-5f) * BloomIntensity;
	}

	return float4(color, 1.0f);
}
