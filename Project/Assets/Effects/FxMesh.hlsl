//////	//////	//////	//////	//////	//////
//	FxMesh - ワールド空間の加算エフェクト		//
//////	//////	//////	//////	//////	//////
// CPU が積んだ三角形（StructuredBuffer、3 頂点 = 1 枚）を、頂点ごとの「形」で塗る。頂点バッファ・インデックスは使わない。
// 色はリニア（RT は SRGB なので、書き込みで sRGB に戻る）。加算（ONE / ONE）なので、出力は「色 × 強さ」。
//
// 深度: MergePass は深度を持たないので、G-buffer の深度を自前で読む（パーティクルと同じ考え方）。
//   見える割合 = saturate((シーンの深さ + DepthBias − 自分の深さ) / SoftRange)（深さはビュー空間の Z、ワールドの長さ）
//   DepthBias: 地面に貼るもの（予兆の線・輪）は少し大きめにして、草や地面の凹凸に埋もれないようにする
//   SoftRange: 物に刺さったところをなだらかに消す幅
//
// 形（ShapeID）と Param の意味（UV は頂点で決める）
//   0 Glow  … uv = 0..1 の四角。中心の光。      Param = (落ち方の指数, 芯の鋭さ, 芯の強さ, -)
//   1 Ring  … uv = 0..1 の四角。輪。            Param = (半径 0..1, 太さ, 内側の塗り, -)
//   2 Star  … uv = 0..1 の四角。十字の煌めき。  Param = (光条の細さ, 光条の減衰, 芯の鋭さ, 回転 rad)
//   3 Trail … u = 古さ 0（新）..1（古）、v = 刃の根元 0..先端 1。 Param = (古さの指数, 先端寄せの指数, 白い芯, 筋)
//   4 Ward  … u = 弧の端 0..1、v = 高さ 0..1。     Param = (強さ, 弾きの猶予 0..1, ひらめき 0..1, -)
//   5 Star5 … uv = 0..1 の四角。五芒星。          Param = (回転 rad, 内側の頂点の半径 0..1, にじみ, -)
//   6 Lane  … u = 始点 0..終点 1、v = 幅方向 0..1。 Param = (塗りの進み 0..1, 長さ（ワールド）, -, 塗りの濃さ)
//   （Lane は以前 4 番で、中を流れる矢印（山形の模様）があった。2026-11-11 に矢印を外して 6 番へ）

struct FxVertex {
	float3 Position;
	uint ShapeID;
	float2 UV;
	float DepthBias;
	float SoftRange;
	float4 Param;
	float4 Color;		// rgb = リニアの色、a = 強さ
};

struct SCENE {
	float4x4 VP;
	float NearZ;
	float FarZ;
	float Time;
	float Padding;
};

StructuredBuffer<FxVertex> Vertices : register(t0, space0);
ConstantBuffer<SCENE> Scene : register(b0, space0);
Texture2D<float> SceneDepth : register(t0, space1);

struct VSOutput {
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
	float4 Param : TEXCOORD1;
	float4 Color : COLOR0;
	float3 Depth : TEXCOORD2;		// x = ビューの Z、y = DepthBias、z = SoftRange
	nointerpolation uint ShapeID : SHAPEID0;
};

VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	FxVertex v = Vertices[vertexID_];

	VSOutput output;
	output.Position = mul(float4(v.Position, 1.0f), Scene.VP);
	output.UV = v.UV;
	output.Param = v.Param;
	output.Color = v.Color;
	output.Depth = float3(output.Position.w, v.DepthBias, v.SoftRange);	// 左手系の透視投影なので w = ビューの Z
	output.ShapeID = v.ShapeID;
	return output;
}

//////	//////	//////	//////	//////	//////

static const float Pi = 3.14159265f;

float Sq(float x_) { return x_ * x_; }

float LinearDepth(float d_) {
	return Scene.NearZ * Scene.FarZ / max(Scene.FarZ - d_ * (Scene.FarZ - Scene.NearZ), 1.0e-6f);
}

float2 Rotate2(float2 p_, float a_) {
	float c = cos(a_);
	float s = sin(a_);
	return float2(p_.x * c - p_.y * s, p_.x * s + p_.y * c);
}

float ShapeGlow(float2 uv_, float4 prm_) {
	float2 p = uv_ * 2.0f - 1.0f;
	float r = length(p);
	float body = pow(saturate(1.0f - r), max(prm_.x, 0.01f));
	float core = exp(-r * r * max(prm_.y, 0.0f)) * saturate((1.0f - r) * 8.0f);
	return body + core * prm_.z;
}

float ShapeRing(float2 uv_, float4 prm_) {
	float2 p = uv_ * 2.0f - 1.0f;
	float r = length(p);
	float band = exp(-Sq((r - prm_.x) / max(prm_.y, 1.0e-3f)));
	float fill = prm_.z * smoothstep(prm_.x, 0.0f, r);
	return (band + fill) * saturate((1.0f - r) * 16.0f);
}

float ShapeStar(float2 uv_, float4 prm_) {
	float2 p = uv_ * 2.0f - 1.0f;
	float r = length(p);
	float2 q = Rotate2(p, prm_.w);
	float2 d = Rotate2(q, Pi * 0.25f);
	float thin = max(prm_.x, 0.1f);
	float fall = max(prm_.y, 0.01f);
	float s = exp(-abs(q.y) * thin - abs(q.x) * fall) + exp(-abs(q.x) * thin - abs(q.y) * fall);
	s += 0.35f * (exp(-abs(d.y) * thin * 1.5f - abs(d.x) * fall * 2.5f) + exp(-abs(d.x) * thin * 1.5f - abs(d.y) * fall * 2.5f));
	float core = exp(-r * r * max(prm_.z, 0.0f));
	return (s + core) * saturate((1.0f - r) * 4.0f);
}

// 戻り値: x = 強さ、y = 白さ
float2 ShapeTrail(float2 uv_, float4 prm_) {
	float u = saturate(uv_.x);
	float v = saturate(uv_.y);
	float age = pow(1.0f - u, max(prm_.x, 0.01f));
	float tip = pow(v, max(prm_.y, 0.01f)) * smoothstep(1.0f, 0.93f, v);
	float streak = 1.0f + prm_.w * sin(u * 37.0f - Scene.Time * 13.0f + v * 9.0f);
	float a = age * tip * streak;
	float white = prm_.z * pow(1.0f - u, 6.0f) * v;
	return float2(a, saturate(white));
}

// 五芒星（距離関数。p は中心が原点、外側の頂点が半径 1）。内側の頂点の半径 = innerRatio
float SdStar5(float2 p_, float innerRatio_) {
	// 1 つの角（36°の扇）に折りたたんでから、外側の頂点と内側の頂点を結ぶ辺までの距離を測る
	const float sector = Pi / 5.0f;
	float ang = atan2(p_.x, p_.y);			// 上向きを 0 に
	float r = length(p_);
	ang = abs(ang - 2.0f * sector * round(ang / (2.0f * sector)));	// [0, 36°]
	float2 q = float2(sin(ang), cos(ang)) * r;
	float2 outerPt = float2(0.0f, 1.0f);
	float2 innerPt = float2(sin(sector), cos(sector)) * innerRatio_;
	float2 e = innerPt - outerPt;
	float2 w = q - outerPt;
	float t = saturate(dot(w, e) / dot(e, e));
	float d = length(w - e * t);
	float side = e.x * w.y - e.y * w.x;		// 辺の内側なら負
	return (side < 0.0f) ? -d : d;
}

// 戻り値: x = 強さ、y = 白さ
float2 ShapeStar5(float2 uv_, float4 prm_) {
	float2 p = Rotate2(float2(uv_.x * 2.0f - 1.0f, 1.0f - uv_.y * 2.0f), prm_.x) / 0.8f;	// 少し余白を残す
	float d = SdStar5(p, clamp(prm_.y, 0.15f, 0.95f));
	float aa = max(fwidth(d), 1.0e-3f);
	float fill = smoothstep(aa, -aa, d);
	float glow = prm_.z * exp(-max(d, 0.0f) * 6.0f) * saturate((1.0f - length(p) * 0.8f) * 4.0f);
	float core = saturate(-d * 3.0f);		// 真ん中ほど白く
	return float2(fill + glow, core * 0.6f * fill);
}

// 予兆の帯: 縁の線＋塗られた部分＋塗りの先頭の光。両端はなめらかに消す（方向を示す模様は入れない）
float ShapeLane(float2 uv_, float4 prm_) {
	float u = uv_.x;
	float v = saturate(uv_.y);
	float progress = saturate(prm_.x);
	float len = max(prm_.y, 0.01f);

	float edge = min(v, 1.0f - v) * 2.0f;		// 縁で 0、真ん中で 1
	float border = exp(-Sq(edge / 0.12f));

	float filled = smoothstep(progress + 0.004f, progress - 0.004f, u);
	float front = (progress < 0.999f) ? exp(-Sq((u - progress) * len / 0.5f)) : 0.0f;
	float ends = smoothstep(0.0f, 0.4f / len, u) * smoothstep(1.0f, 1.0f - 0.8f / len, u);

	float inside = border + prm_.w;
	float outside = border * 0.3f;
	return (lerp(outside, inside, filled) + front * 1.4f) * ends;
}

// 戻り値: x = 強さ、y = 白さ
float2 ShapeWard(float2 uv_, float4 prm_) {
	float u = saturate(uv_.x);
	float v = saturate(uv_.y);
	float edgeU = exp(-Sq(min(u, 1.0f - u) / 0.05f));
	float vFade = smoothstep(0.0f, 0.12f, v) * smoothstep(1.0f, 0.55f, v);
	float scan = pow(0.5f + 0.5f * sin(v * 40.0f - Scene.Time * 5.0f), 6.0f);
	float grid = pow(abs(cos(u * Pi * 14.0f)), 40.0f);
	float base = 0.15f + 0.45f * scan + 0.35f * grid + 0.9f * edgeU;
	float a = vFade * (prm_.x * base + prm_.y * (0.5f + 0.8f * edgeU + 0.4f * scan) + prm_.z * 1.6f);
	float white = saturate(prm_.y * 0.35f + prm_.z * 0.8f);
	return float2(a, white);
}

float4 PSMain(VSOutput input_) : SV_Target0 {
	float sceneZ = LinearDepth(SceneDepth.Load(int3(input_.Position.xy, 0)));
	float visible = saturate((sceneZ + input_.Depth.y - input_.Depth.x) / max(input_.Depth.z, 1.0e-3f));

	float a = 0.0f;
	float white = 0.0f;
	switch (input_.ShapeID) {
		case 0U: a = ShapeGlow(input_.UV, input_.Param); break;
		case 1U: a = ShapeRing(input_.UV, input_.Param); break;
		case 2U: a = ShapeStar(input_.UV, input_.Param); break;
		case 3U: { float2 t = ShapeTrail(input_.UV, input_.Param); a = t.x; white = t.y; } break;
		case 4U: { float2 w = ShapeWard(input_.UV, input_.Param); a = w.x; white = w.y; } break;
		case 5U: { float2 st = ShapeStar5(input_.UV, input_.Param); a = st.x; white = st.y; } break;
		case 6U: a = ShapeLane(input_.UV, input_.Param); break;
		default: break;
	}

	float3 rgb = lerp(input_.Color.rgb, float3(1.0f, 0.97f, 0.9f), white);
	float3 result = rgb * max(a, 0.0f) * max(input_.Color.a, 0.0f) * visible;
	return float4(max(result, 0.0f), 0.0f);
}
