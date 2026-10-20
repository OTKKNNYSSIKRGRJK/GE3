//////	//////	//////	//////	//////	//////
//	VsBanner（Lumina.VsBanner）				//
//////	//////	//////	//////	//////	//////
// バトル開始の合図のバナー 1 本。画面ピクセル空間（原点 = 画面中心、y 上）で配置を受け取る。
// VSMain : SV_VertexID から帯のクアッドを生成（頂点バッファなし）。アンチエイリアスのにじみ分（Margin）だけ広げる
// PSMain : BannerDistance（px 単位の符号付き距離、内側が負）から形と縁取りを作り、中身（キャラの寄りの RT）を重ねる。
//          プリマルチプライド α で出力
// 中身は「帯の中心からの画面座標（回転なし）」で貼る。帯と一緒に滑るが、絵は傾かない。
// 中身の RT の縁 ContentFeather px は α を落とす（滑り込み・抜けの最中に RT の端が帯にかかっても線にならないように）
//
// 形を変えたいときは BannerDistance だけ差し替える（SDF テクスチャを読む、など）。縁の演出は d から作る

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
	float2 Padding0;
};

Texture2D<float4> ContentTexture : register(t0);	// キャラの寄り（SRGB の RT。サンプルするとリニア。α = キャラのいる所）
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

// 帯の形（px 単位の符号付き距離、内側が負）。今は角の丸い長方形
float BannerDistance(float2 p_) {
	const float r = CornerRadius;
	const float2 q = abs(p_) - (HalfSize - r);
	return length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - r;
}

float4 PSMain(VSOutput input_) : SV_Target {
	const float d = BannerDistance(input_.Local);
	const float aa = max(fwidth(d), 1.0e-4f);

	float fill = saturate(0.5f - d / aa);

	float edge = 0.0f;
	if (EdgeWidth > 0.0f) {
		// 縁から内側へ EdgeWidth の帯（-EdgeWidth <= d <= 0）
		const float band = abs(d + EdgeWidth * 0.5f) - EdgeWidth * 0.5f;
		edge = saturate(0.5f - band / aa);
	}

	float3 rgb = Color;

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
		rgb = lerp(rgb, content.rgb, content.a * ContentOpacity * feather);
	}

	// 縁取りは中身の上に重ねる
	rgb = lerp(rgb, EdgeColor, edge);

	if (DebugView > 1.5f) {
		// 距離の等高線（16 px ごと）
		const float c = frac(-d / 16.0f);
		rgb = float3(0.15f, 0.45f, 1.0f) * (0.35f + 0.65f * c);
	}
	else if (DebugView > 0.5f) {
		rgb = insideContent ? float3(contentUV, 0.0f) : float3(0.05f, 0.05f, 0.05f);
	}

	const float alpha = fill * Opacity;
	return float4(rgb * alpha, alpha);
}
