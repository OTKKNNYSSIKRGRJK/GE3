//////	//////	//////	//////	//////	//////
//	GameOverGrade（ゲームオーバーの色抜け）	//
//////	//////	//////	//////	//////	//////
// UI まで描き終えた画面（中間 RT。SRGB なので読むとリニア）を、バックバッファ（SRGB）へフルスクリーンで写す。
// 1 本の PS で：（ぼかし / 回転ぼかし）→ グラデーションマップ（Clip Studio 風）→ ビネット
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

	float ChannelSplitPixels;		// R を +、B を − へずらす量（px。0 ならスキップ）。脈動と包絡は CPU 側で掛けてある
	float ChannelSplitRadial;		// 0 = 一定の向き、1 = 画面の中心から放射状
	float2 ChannelSplitDir;			// 一定の向きのときのずれの向き（単位ベクトル）

	float BlurPixels;				// 背景のぼかし半径（px。0 ならスキップ）
	float2 ChannelSplitFocus;		// チャンネルスプリットの中心（キャラの画面上の位置。UV）
	float ChannelSplitFocusRadius;	// この距離（px）で、ずれが ChannelSplitFar まで弱まる

	float ChannelSplitFar;			// 遠くに残るずれの割合
	float ChannelSplitCenterSoft;	// キャラの真上では向きが定まらないので、この距離（px）以内ではずれを小さくする
	float SpinAngle;				// 回転ぼかしの振れ幅（ラジアン。0 ならスキップ）。巻き戻しのあいだ CPU 側が包絡を掛けて入れる
	float SpinFocus;				// 回転の中心：0 = 画面の中心、1 = ChannelSplitFocus（キャラの位置）
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
//	Blur / shading							//
//////	//////	//////	//////	//////	//////

static const uint BlurTaps = 48U;

float3 SampleLinear(float2 uv_) {
	return SceneTexture.SampleLevel(LinearClamp, uv_, 0.0f).rgb;
}

// 背景のぼかし（リニア空間）：黄金角のらせん状に BlurTaps 点を平均する。ピクセルごとに回して、段をごく細かいノイズに変える
float3 BlurredLinear(float2 uv_, float2 invSize_, float2 seed_) {
	const float rotation = Hash12(seed_) * 6.2831853f;
	float3 sum = float3(0.0f, 0.0f, 0.0f);
	[loop]
	for (uint i = 0U; i < BlurTaps; ++i) {
		const float radius = sqrt((float(i) + 0.5f) / float(BlurTaps)) * BlurPixels;
		const float angle = float(i) * 2.3999632f + rotation;
		float s, c;
		sincos(angle, s, c);
		sum += SampleLinear(uv_ + float2(c, s) * radius * invSize_);
	}
	return sum / float(BlurTaps);
}

static const uint SpinTaps = 32U;

// 回転ぼかし（リニア空間）：中心のまわりに SpinAngle の幅で回しながら SpinTaps 点を平均する（外側ほど長く流れる）。
// 背景のぼかし（BlurPixels > 0）も同時に効いているときは、各点に らせん状のずれを足して 1 回でまとめる（点数を増やさない）。
// 点の角度とらせんの向きはピクセルごとにずらして、段をごく細かいノイズに変える
float3 SpinBlurredLinear(float2 uv_, float2 invSize_, float2 seed_) {
	const float2 size = 1.0f / invSize_;
	const float2 center = lerp(float2(0.5f, 0.5f), ChannelSplitFocus, saturate(SpinFocus));
	const float2 p = (uv_ - center) * size;		// 中心からの px
	const float jitter = Hash12(seed_ + 17.0f);
	const float rotation = Hash12(seed_) * 6.2831853f;
	const bool withBlur = BlurPixels > 0.01f;
	float3 sum = float3(0.0f, 0.0f, 0.0f);
	[loop]
	for (uint i = 0U; i < SpinTaps; ++i) {
		const float f = (float(i) + jitter) / float(SpinTaps) - 0.5f;		// -0.5..0.5
		float s, c;
		sincos(f * SpinAngle, s, c);
		float2 q = float2(c * p.x - s * p.y, s * p.x + c * p.y);
		[branch]
		if (withBlur) {
			const float radius = sqrt((float(i) + 0.5f) / float(SpinTaps)) * BlurPixels;
			const float angle = float(i) * 2.3999632f + rotation;
			float bs, bc;
			sincos(angle, bs, bc);
			q += float2(bc, bs) * radius;
		}
		sum += SampleLinear(center + q * invSize_);
	}
	return sum / float(SpinTaps);
}

// 1 点ぶんの色（sRGB）：ぼかし → グラデーションマップ → ビネット
float3 ShadeAt(float2 uv_, float2 invSize_, float2 seed_) {
	float3 sourceLinear;
	[branch]
	if (abs(SpinAngle) > 1.0e-4f) {
		sourceLinear = SpinBlurredLinear(uv_, invSize_, seed_);
	}
	else if (BlurPixels > 0.01f) {
		sourceLinear = BlurredLinear(uv_, invSize_, seed_);
	}
	else {
		sourceLinear = SampleLinear(uv_);
	}
	const float3 source = LinearToSRGB(sourceLinear);

	const float luma = dot(source, float3(0.2126f, 0.7152f, 0.0722f));
	float t = saturate((luma - BlackPoint) / max(WhitePoint - BlackPoint, 1.0e-4f));
	t = pow(t, 1.0f / max(Gamma, 1.0e-3f));

	float3 color = lerp(source, EvaluateGradient(t), GradientAmount);

	if (ViewLuma > 0.5f) {
		color = t.xxx;
	}
	else {
		color = lerp(color, VignetteColor, saturate(Vignette(uv_) * VignetteAmount));
	}
	return color;
}

// 出力ピクセル uv_ でのチャンネルスプリットのずれ（px）。キャラの画面上の位置に近いほど大きく、離れるほど ChannelSplitFar まで弱まる。
// 向きは、ChannelSplitRadial = 1 でキャラから離れる向き（真上では向きが定まらないので、CenterSoft 以内で小さくする）、0 で一定の向き
float2 ChannelSplitShiftPx(float2 uv_, float2 size_) {
	const float2 fromFocus = (uv_ - ChannelSplitFocus) * size_;
	const float len = length(fromFocus);
	const float2 away = fromFocus / max(len, max(ChannelSplitCenterSoft, 1.0f));
	const float2 direction = lerp(ChannelSplitDir, away, ChannelSplitRadial);
	const float proximity = 1.0f - smoothstep(0.0f, max(ChannelSplitFocusRadius, 1.0f), len);
	const float weight = lerp(ChannelSplitFar, 1.0f, proximity);
	return direction * (ChannelSplitPixels * weight);
}

//////	//////	//////	//////	//////	//////
//	PSMain									//
//////	//////	//////	//////	//////	//////

float4 PSMain(VSOutput input_) : SV_Target {
	uint width, height;
	SceneTexture.GetDimensions(width, height);
	const float2 size = float2(width, height);
	const float2 invSize = 1.0f / size;
	const float2 seed = input_.Position.xy;

	float3 color;
	[branch]
	if (abs(ChannelSplitPixels) > 0.01f) {
		// チャンネルスプリット：R は+、B は−へずらす（キャラに近いほど大きく、放射状ならキャラから離れる向き）。
		// 色抜けのあとの色でずらすので、モノクロの絵でも赤と青の縁が出る
		const float2 shiftPx = ChannelSplitShiftPx(input_.UV, size);
		const float2 shiftUV = shiftPx * invSize;
		const float r = ShadeAt(input_.UV + shiftUV, invSize, seed).r;
		const float g = ShadeAt(input_.UV, invSize, seed).g;
		const float b = ShadeAt(input_.UV - shiftUV, invSize, seed).b;
		color = float3(r, g, b);
	}
	else {
		color = ShadeAt(input_.UV, invSize, seed);
	}

	// 比較用：UV.x が SplitPosition より左は元の絵
	if (input_.UV.x < SplitPosition) {
		color = LinearToSRGB(SampleLinear(input_.UV));
	}

	color += (Hash12(input_.Position.xy) - 0.5f) * DitherAmount;
	return float4(SRGBToLinear(color), 1.0f);
}
