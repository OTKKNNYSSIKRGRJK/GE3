//////	//////	//////	//////	//////	//////
//	TitlePost（Title のポスト処理）			//
//////	//////	//////	//////	//////	//////
// Title 用 Canvas（SRGB。読むとリニア）を、バックバッファ（SRGB）へフルスクリーンで写す。
// 1 本の PS で：ズームブラー（Center へ向かって）＋ 色収差（画面端ほど RGB をずらす）
//              ＋ 簡易ブルーム（黄金角スパイラル。ピクセルごとに回転をずらす）＋ 白ビネット
// 効果がすべて 0 のときはそのまま写す（Load 1 回）。
// VSMain : SV_VertexID から画面全体を覆う三角形を 1 枚生成（頂点バッファなし）

cbuffer Constants : register(b0) {
	float2 Center;			// ズームブラーの中心（画面 UV、左上が 0,0）
	float ZoomBlur;			// 中心へ向かってぼかす長さ（中心までの距離に対する割合）
	float ZoomTaps;			// 1..MaxZoomTaps
	float Chromatic;		// 画面端での RGB のずれ（UV）
	float BloomIntensity;
	float BloomRadiusPx;
	float BloomThreshold;	// これより明るい分（リニア）だけにじませる
	float3 VignetteColor;	// リニア
	float VignetteAmount;
	float VignetteRadius;	// 画面の高さを 1 とした、画面中心からの距離
	float VignetteSoftness;
	float BloomTaps;		// 1..MaxBloomTaps
	float Aspect;			// 幅 / 高さ
	float2 Resolution;		// px
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

float3 SampleScene(float2 uv_) {
	return SceneTexture.SampleLevel(LinearClamp, uv_, 0.0f).rgb;
}

// 色収差：R を外側、B を内側へずらす（G はそのまま）
float3 SampleChromatic(float2 uv_, float2 offset_) {
	return float3(
		SampleScene(uv_ + offset_).r,
		SampleScene(uv_).g,
		SampleScene(uv_ - offset_).b
	);
}

//////	//////	//////	//////	//////	//////
//	Pixel shader							//
//////	//////	//////	//////	//////	//////

float4 PSMain(VSOutput input_) : SV_Target {
	// * 効果がすべて 0 なら、そのまま写す（通常時のコストを抑える）
	if (ZoomBlur <= 0.0f && Chromatic <= 0.0f && BloomIntensity <= 0.0f && VignetteAmount <= 0.0f) {
		return float4(SceneTexture.Load(int3(input_.Position.xy, 0)).rgb, 1.0f);
	}

	const float2 uv = input_.UV;
	const float2 fromScreenCenter = uv - 0.5f;
	const float distanceFromCenter = length(fromScreenCenter * float2(Aspect, 1.0f));	// 画面の高さ = 1

	// * 色収差：画面中心から外向きに、端ほど大きく（2 乗）
	const float edge = saturate(distanceFromCenter / (0.5f * length(float2(Aspect, 1.0f))));
	const float2 direction = fromScreenCenter / max(length(fromScreenCenter), 1e-5f);
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
	//   タップが少ないので、ピクセルごとにスパイラルを回して縞をノイズに変える
	if (BloomIntensity > 0.0f) {
		const uint bloomTaps = clamp((uint)BloomTaps, 1U, MaxBloomTaps);
		const float rotation = Hash12(input_.Position.xy + 17.0f) * TwoPi;
		const float2 radiusUV = max(BloomRadiusPx, 0.0f) / Resolution;

		float3 bloom = float3(0.0f, 0.0f, 0.0f);
		float weightSum = 0.0f;
		[loop]
		for (uint i = 0U; i < bloomTaps; ++i) {
			const float r = sqrt(((float)i + 0.5f) / (float)bloomTaps);
			const float angle = (float)i * GoldenAngle + rotation;
			const float2 offset = float2(cos(angle), sin(angle)) * r * radiusUV;
			const float3 bright = max(SampleScene(uv + offset) - BloomThreshold, 0.0f);
			const float weight = exp(-3.0f * r * r);
			bloom += bright * weight;
			weightSum += weight;
		}
		color += bloom / max(weightSum, 1e-5f) * BloomIntensity;
	}

	// * 白ビネット：画面中心から離れるほど VignetteColor へ
	const float vignette = smoothstep(VignetteRadius, VignetteRadius + max(VignetteSoftness, 1e-3f), distanceFromCenter);
	color = lerp(color, VignetteColor, saturate(vignette * VignetteAmount));

	return float4(color, 1.0f);
}
