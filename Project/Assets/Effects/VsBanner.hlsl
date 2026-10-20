// バトル開始の合図のバナー 1 本。画面ピクセル空間（原点 = 画面中心、y 上）で配置を受け取る。
// VSMain : SV_VertexID から帯のクアッドを生成（頂点バッファなし）。アンチエイリアスのにじみ分（Margin）だけ広げる
// PSMain : BannerDistance（px 単位の符号付き距離、内側が負）から形と縁取りを作り、中身（キャラの寄りの RT）を重ねる。
//          プリマルチプライド α で出力
// 中身は「帯の中心からの画面座標（回転なし）」で貼る。帯と一緒に滑るが、絵は傾かない。
// 中身の RT の縁 ContentFeather px は α を落とす（滑り込み・抜けの最中に RT の端が帯にかかっても線にならないように）
//
// 形: BannerDistance。HasShape なら SDF テクスチャ（Lumina.SDF で画像から作った R8G8。R = 距離、G = 元の α）、無ければ角の丸い長方形。
//     縁の演出は d から作る。SDF は内外の 2 値なので、半透明・濃淡は G（opacity）で別に掛ける
// 背景: ArtEnabled ならベタ塗りの色にシェーダアート（Fancy.CS の移植。時間とコサインパレットだけ受け取る）を薄く混ぜる
// 文字（V / S）も同じシェーダで描く（中身なし・アートなし・回さない帯）
// グロー: 形の外側（d > 0）に加算でにじませる。幅は C++ 側で SDF の距離の範囲までに抑えてある

cbuffer Constants : register(b0) {
	float2 Center;			// 帯の中心（px）
	float2 Axis;			// 帯の向き（単位ベクトル）。法線は (-Axis.y, Axis.x)
	float2 HalfSize;		// (長さ, 太さ) / 2（px）
	float2 InvHalfScreen;	// (2 / 幅, 2 / 高さ)。px → NDC
	float3 Color;			// リニア
	float Opacity;
	float3 EdgeColor;		// リニア
	float EdgeWidth;		// 縁取りの太さ（内側へ、px）
	float HasContent;		// 1 = 中身を貼る
	float DebugView;		// 0 = 通常、1 = 中身の UV、2 = 距離の等高線
	float CornerRadius;		// 角の丸み（px）
	float Margin;			// クアッドを広げる量（px）
	float2 ContentOffset;	// 中身の中心の、帯の中心からのずらし（画面 px、y 上）。帯が止まったとき中身の中心が画面の中心に来る値
	float2 ContentSize;		// 中身の画面上の大きさ（px）
	float ContentOpacity;
	float ContentFeather;	// 中身の縁で α を落とす幅（px）
	float HasShape;			// 1 = 形を SDF テクスチャから取る
	float ShapeRange;		// SDF に詰めた距離の範囲（テクセル）
	float2 ShapeTexels;		// SDF テクスチャの大きさ
	float ShapeUseOpacity;	// 1 = 塗りに G（元の α）を掛ける
	float EdgeUseOpacity;	// 1 = 縁取りにも G を掛ける
	float3 PaletteBias;		// コサインパレット: 色(t) = Bias + Amplitude · cos(2π · (Frequency · t + Phase))
	float ArtTime;			// シェーダアートの時間（秒 × Speed）
	float3 PaletteAmplitude;
	float ArtScale;			// 模様の 1 単位（元の「16 texel」）が帯の何 px か
	float3 PaletteFrequency;
	float ArtEnabled;		// 1 = シェーダアート、0 = ベタ塗り（Color）
	float3 PalettePhase;
	float GlowWidth;		// グローの幅（px）。0 で無し
	float3 GlowColor;		// リニア
	float GlowIntensity;
	float2 ArtOrigin;		// 模様の中心の、帯の中心からのずらし（px、x = 帯の向き、y = 下向き）
	float GlowFalloff;		// (1 − d / 幅)^Falloff
	float ArtOutputIsGamma;	// 1 = 元の出力をガンマのままの値とみなして 0..1 に収め、リニアに直す
};

static const float TwoPi = 6.28318530718f;

Texture2D<float4> ContentTexture : register(t0);	// キャラの寄り（SRGB の RT。サンプルするとリニア。α = キャラのいる所）
Texture2D<float2> ShapeTexture : register(t1);		// バナーの形（R8G8）。R = SDF（v = 0.5 − d / (2·range)、内側が 0.5 より大きい）、G = 元の α
SamplerState LinearClamp : register(s0);

struct VSOutput {
	float4 Position : SV_Position;
	float2 Local : TEXCOORD0;		// 帯のローカル座標（px）。x = 帯の向き、y = 法線の向き
	float2 Screen : TEXCOORD1;		// 帯の中心からの画面座標（px、y 上、回転なし）
};

VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	static const float2 Corners[6] = {
		float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
		float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f),
	};

	const float2 local = Corners[vertexID_] * (HalfSize + Margin);
	const float2 normal = float2(-Axis.y, Axis.x);
	const float2 position = Center + Axis * local.x + normal * local.y;

	VSOutput output;
	output.Position = float4(position * InvHalfScreen, 0.0f, 1.0f);
	output.Local = local;
	output.Screen = position - Center;
	return output;
}

// 角の丸い長方形（px 単位の符号付き距離、内側が負）
float BoxDistance(float2 p_, float r_) {
	const float2 q = abs(p_) - (HalfSize - r_);
	return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r_;
}

// 帯の形（px 単位の符号付き距離、内側が負）。opacity_ = 画像の元の α（形が無いときは 1）
float BannerDistance(float2 p_, out float opacity_) {
	opacity_ = 1.0f;
	if (HasShape < 0.5f) { return BoxDistance(p_, CornerRadius); }

	// 画像は帯の長方形（2·HalfSize）にぴったり貼る。右端 = +x（帯の向き = 進行方向）、上端 = +y（法線）
	const float2 uv = float2(p_.x / (2.0f * HalfSize.x) + 0.5f, 0.5f - p_.y / (2.0f * HalfSize.y));
	const float2 texel = ShapeTexture.SampleLevel(LinearClamp, saturate(uv), 0.0f);
	const float v = texel.r;
	opacity_ = texel.g;
	// テクセル → px（縦横の比率がそろっている前提で平均）
	const float2 texelPx = (2.0f * HalfSize) / max(ShapeTexels, 1.0f);
	const float dShape = (0.5f - v) * 2.0f * ShapeRange * 0.5f * (texelPx.x + texelPx.y);
	// 長方形の外は必ず外側（Clamp で端の値が伸びても、クアッドの余白に形がはみ出さないように）
	return max(dShape, BoxDistance(p_, 0.0f));
}

//////	//////	//////	//////	//////	//////
//	背景のシェーダアート（Fancy.CS の移植）	//
//////	//////	//////	//////	//////	//////

float3 PaletteColor(float t_) {
	return PaletteBias + PaletteAmplitude * cos(TwoPi * (PaletteFrequency * t_ + PalettePhase));
}

float2x2 Rotate2D(float t_) {
	return float2x2(
		cos(t_), sin(t_),
		-sin(t_), cos(t_)
	);
}

float Box3(float3 p_, float3 b_) {
	const float3 d = abs(p_) - b_;
	return min(max(d.x, max(d.y, d.z)), 0.0f) + length(max(d, 0.0f));
}

// 元のシェーダ（コンピュートシェーダ）は 320x180 のテクスチャに、中心からの texel / 16 · 脈動 · 0.125 で UV を作っていた。
// ここでは帯の中心（＋ Origin）からの px（y 下向き）/ Scale を「texel / 16」の代わりにする。
// 帯のローカル座標なので、帯と一緒に傾いて滑る（B は 180° 回るので点対称）。それ以外は元の式のまま
float3 BackgroundArtColor(float2 local_) {
	const float t = ArtTime;
	const float2 px = float2(local_.x, -local_.y) - ArtOrigin;
	float2 uv = px / ArtScale * (sin(t * 2.5f) * 0.1f + 0.5f) * 0.125f;
	const float len0 = length(uv);

	float3 color = float3(0.05f, 0.1f, 0.05f);
	uv = mul(uv, Rotate2D(atan2(uv.y, uv.x) * 1.0f));

	[unroll]
	for (int n = 0; n < 2; ++n) {
		const float i = (float)n;
		uv = mul(uv, Rotate2D(atan2(uv.y, uv.x) * 0.5f));
		uv = frac(uv * 16.0f) - 0.5f;

		float d = Box3(float3(uv, 1.0f), float3(0.15f, 0.15f, 0.75f)) * exp(-len0 * 1.5f);
		d += Box3(float3(uv * 0.75f, 1.0f), float3(0.3f, 0.3f, 0.75f)) * exp(-len0 * 2.5f);
		d = sin(d * 16.0f + t * 3.5f) * 0.1f;
		d = abs(d) + 0.002f;
		d = pow(0.01f / d, 1.5f);

		color += PaletteColor(len0 + i * 2.5f + t * 1.1f) * d;
	}

	// 元はテクスチャにそのまま書いていた。ガンマのままの値とみなすなら 0..1 に収めてリニアに直す
	return (ArtOutputIsGamma > 0.5f) ? pow(saturate(color), 2.2f) : color;
}

float4 PSMain(VSOutput input_) : SV_Target {
	float shapeOpacity = 1.0f;
	const float d = BannerDistance(input_.Local, shapeOpacity);
	const float aa = max(fwidth(d), 1.0e-4f);

	const float fill = saturate(0.5f - d / aa);
	const float bodyOpacity = (ShapeUseOpacity > 0.5f) ? shapeOpacity : 1.0f;

	float edge = 0.0f;
	if (EdgeWidth > 0.0f) {
		// 縁から内側へ EdgeWidth の帯（-EdgeWidth <= d <= 0）
		const float band = abs(d + EdgeWidth * 0.5f) - EdgeWidth * 0.5f;
		edge = saturate(0.5f - band / aa);
	}

	// 背景のアートは塗りの色に薄く混ぜる（0.125）。ArtEnabled = 0（文字・アートを切ったとき）はベタ塗り
	float3 rgb = (ArtEnabled > 0.5f) ? (BackgroundArtColor(input_.Local) * 0.125f + Color * 0.875f) : Color;

	// 中身の UV（左上 0,0）。画面の向きのまま（帯の回転は掛けない）
	const float2 rel = input_.Screen - ContentOffset;
	const float2 contentUV = float2(rel.x / max(ContentSize.x, 1.0e-4f) + 0.5f, 0.5f - rel.y / max(ContentSize.y, 1.0e-4f));
	const bool insideContent = all(contentUV >= 0.0f) && all(contentUV <= 1.0f);

	// UV を 0..1 に収めて読み、範囲外（insideContent でない所）は使わない
	const float4 content = ContentTexture.SampleLevel(LinearClamp, saturate(contentUV), 0.0f);
	if (HasContent > 0.5f && insideContent) {
		// RT の縁までの距離（px）で α を落とす
		const float2 toEdge = min(contentUV, 1.0f - contentUV) * ContentSize;
		const float feather = (ContentFeather > 0.0f) ? smoothstep(0.0f, ContentFeather, min(toEdge.x, toEdge.y)) : 1.0f;
		// 中身はプリマルチプライド（キャラは α = 1、加算のパーティクルは α = 0 で色だけ足してある）
		const float k = ContentOpacity * feather;
		rgb = rgb * (1.0f - content.a * k) + content.rgb * k;
	}

	// 縁取りは中身の上に重ねる（プリマルチプライドで: 縁取り over 塗り）
	const float bodyA = fill * bodyOpacity;
	const float edgeA = edge * fill * ((EdgeUseOpacity > 0.5f) ? bodyOpacity : 1.0f);
	float3 premul = EdgeColor * edgeA + rgb * bodyA * (1.0f - edgeA);
	float alpha = edgeA + bodyA * (1.0f - edgeA);

	// 外側のグロー（加算。α は増やさないので、下の画面に光が足される）
	if (GlowWidth > 0.0f) {
		const float outside = max(d, 0.0f);
		const float glow = GlowIntensity * pow(saturate(1.0f - outside / GlowWidth), GlowFalloff) * (1.0f - fill);
		premul += GlowColor * glow;
	}

	if (DebugView > 1.5f) {
		// 距離の等高線（16 px ごと）。濃さは無視して形全体を見せる
		const float c = frac(-d / 16.0f);
		premul = float3(0.15f, 0.45f, 1.0f) * (0.35f + 0.65f * c) * fill;
		alpha = fill;
	}
	else if (DebugView > 0.5f) {
		premul = (insideContent ? float3(contentUV, 0.0f) : float3(0.05f, 0.05f, 0.05f)) * fill;
		alpha = fill;
	}

	return float4(premul, alpha) * Opacity;
}
