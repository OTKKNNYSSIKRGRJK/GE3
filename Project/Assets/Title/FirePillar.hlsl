//////	//////	//////	//////	//////	//////
//	FirePillar (prototype)					//
//////	//////	//////	//////	//////	//////
// 行優先（-Zpr）・行ベクトル（mul(v, M)）前提
// VSMain : SV_VertexID から円柱を生成（頂点バッファなし）
// PSMain : テクスチャ2層（歪み＋本体）→ 侵食 → フェード → 3色ランプ → プリマルチプライド出力

cbuffer Constants : register(b0) {
	float4x4 World;
	float4x4 ViewProj;

	float3 CameraPos;	float Time;
	float Radius;		float Height;		float TopScale;		float ShapeCurve;
	float WobbleAmp;	float WobbleFreq;	float WobbleWave;	float Twist;
	float NoiseScaleU;	float NoiseScaleV;	float NoiseSpeed;	float WarpAmount;
	float WarpScaleU;	float WarpScaleV;	float WarpSpeed;	float Seed;
	float CutBottom;	float CutTop;		float CutCurve;		float Softness;
	float FadeBottom;	float FadeTopStart;	float RimPower;		float BackDarken;
	float Intensity;	float Opacity;		float Additive;		float TextureColor;
	float3 RampLow;		float HeatScale;
	float3 RampMid;		float HeatHeight;
	float3 RampHigh;	float RampMidPos;
	uint NumSegmentsU;	uint NumSegmentsV;	uint Padding0;		uint Padding1;
};

Texture2D<float4> FlameTexture : register(t0);
SamplerState LinearWrap : register(s0);

static const float TwoPi = 6.28318530718f;

struct VSOutput {
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;			// x: 円周 0..1 / y: 高さ 0..1
	float3 WorldPos : POSITION0;
	float3 WorldNormal : NORMAL0;
};

// 1 クアッド = 2 三角形。(u, v) のオフセット
// 外から見て時計回り（D3D 既定の表面）になる並び
static const uint2 QuadCorners[6] = {
	uint2(0U, 0U), uint2(0U, 1U), uint2(1U, 0U),
	uint2(1U, 0U), uint2(0U, 1U), uint2(1U, 1U),
};

VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	const uint quadID = vertexID_ / 6U;
	const uint2 corner = QuadCorners[vertexID_ % 6U];
	const uint idxU = quadID % NumSegmentsU + corner.x;
	const uint idxV = quadID / NumSegmentsU + corner.y;
	const float u = (float)idxU / (float)NumSegmentsU;	// 最後の列は u = 1（継ぎ目の頂点を共有しない）
	const float v = (float)idxV / (float)NumSegmentsV;

	const float theta = u * TwoPi;
	const float2 dir = float2(cos(theta), sin(theta));

	const float radius = Radius * lerp(1.0f, TopScale, pow(v, ShapeCurve));

	// 軸の横揺れ。根元は固定で上ほど大きく
	const float2 sway = float2(
		sin(Time * WobbleFreq + v * WobbleWave * TwoPi),
		cos(Time * WobbleFreq * 0.83f + v * WobbleWave * TwoPi * 1.3f)
	) * (WobbleAmp * v);

	const float3 localPos = float3(
		dir.x * radius + sway.x,
		v * Height,
		dir.y * radius + sway.y
	);
	const float4 worldPos = mul(float4(localPos, 1.0f), World);

	VSOutput output;
	output.Position = mul(worldPos, ViewProj);
	output.UV = float2(u, v);
	output.WorldPos = worldPos.xyz;
	output.WorldNormal = mul(float3(dir.x, 0.0f, dir.y), (float3x3)World);
	return output;
}

float3 SampleRamp(float t_) {
	return (t_ < RampMidPos)
		? lerp(RampLow, RampMid, t_ / RampMidPos)
		: lerp(RampMid, RampHigh, (t_ - RampMidPos) / (1.0f - RampMidPos));
}

float4 PSMain(VSOutput input_, bool isFrontFace_ : SV_IsFrontFace) : SV_Target0 {
	const float u = input_.UV.x;
	const float v = input_.UV.y;

	// 柱ごとの UV オフセット（UV は WRAP なので小数部だけで十分）
	const float2 seedOffset = frac(Seed * float2(0.61803398f, 0.38196601f));

	// ねじり：高さに応じて U をずらす。ScaleU が整数なら u=0 と u=1 で継ぎ目は一致する
	const float twistedU = u + v * Twist;

	// 歪み層
	const float2 warpUV = float2(twistedU * WarpScaleU, v * WarpScaleV - Time * WarpSpeed) + seedOffset;
	const float warp = FlameTexture.Sample(LinearWrap, warpUV).a - 0.5f;

	// 本体層（歪み層の値で UV を揺らす）
	const float2 noiseUV =
		float2(twistedU * NoiseScaleU, v * NoiseScaleV - Time * NoiseSpeed) +
		seedOffset.yx +
		warp * WarpAmount;
	const float4 tex = FlameTexture.Sample(LinearWrap, noiseUV);
	const float mask = tex.a;	// α はリニア（SRGB フォーマットでも変換されない）

	// 侵食：上ほどしきい値を上げて舌状に削る
	const float threshold = lerp(CutBottom, CutTop, pow(v, CutCurve));
	float alpha = smoothstep(threshold - Softness, threshold + Softness, mask);

	// 円柱っぽさを消す：輪郭（視線に対して接線方向の面）と上下端
	const float3 toCamera = normalize(CameraPos - input_.WorldPos);
	const float facing = abs(dot(normalize(input_.WorldNormal), toCamera));
	const float rim = pow(facing, RimPower);
	const float heightFade = smoothstep(0.0f, FadeBottom, v) * (1.0f - smoothstep(FadeTopStart, 1.0f, v));
	alpha *= rim * heightFade * Opacity;

	// 色：しきい値をどれだけ上回ったか（＝炎の内側ほど）＋ 根元ほど熱い
	const float heat = saturate((mask - threshold) * HeatScale + (1.0f - v) * HeatHeight);
	float3 color = SampleRamp(heat);
	color *= lerp(float3(1.0f, 1.0f, 1.0f), tex.rgb, TextureColor);
	color *= isFrontFace_ ? 1.0f : BackDarken;
	color *= Intensity;

	// プリマルチプライドアルファ：Additive = 1 で加算、0 で通常の半透明
	return float4(color * alpha, alpha * (1.0f - Additive));
}
