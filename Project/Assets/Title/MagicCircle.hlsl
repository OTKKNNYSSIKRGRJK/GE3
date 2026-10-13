//////	//////	//////	//////	//////	//////
//	MagicCircle (prototype)					//
//////	//////	//////	//////	//////	//////
// 行優先（-Zpr）・行ベクトル（mul(v, M)）前提
// VSMain : SV_VertexID から地面に水平なクアッドを生成（頂点バッファなし）
// PSMain : 正規化極座標で各部品の距離 d を求め、線（芯）＋グローに変換して加算出力
//          長さの単位はすべて「半径 = 1」の正規化単位

cbuffer Constants : register(b0) {
	float4x4 ViewProj;

	float3 Center;		float Radius;
	float Height;		float Angle;		float Reveal;			float Intensity;
	float3 Color;		float LineWidth;
	float GlowAmount;	float GlowWidth;	float FillAmount;		float Extent;
	float Ring0;		float Ring1;		float PolygonRadius;	float NodeRadius;
	float TickInner;	float TickOuter;	float SpokeInner;		float SpokeOuter;
	float RuneInner;	float RuneOuter;	float RuneDensity;		float RuneShimmer;
	uint Elements;		uint PolygonVertices;	uint PolygonStep;	uint TickCount;
	uint SpokeCount;	uint RuneCount;		uint RuneSeed;			uint Padding0;
	float Time;			float Padding1;		float Padding2;			float Padding3;
};

#define ELEMENT_RING0	(1U << 0U)
#define ELEMENT_RING1	(1U << 1U)
#define ELEMENT_POLYGON	(1U << 2U)
#define ELEMENT_NODES	(1U << 3U)
#define ELEMENT_TICKS	(1U << 4U)
#define ELEMENT_SPOKES	(1U << 5U)
#define ELEMENT_RUNES	(1U << 6U)
#define ELEMENT_FILL	(1U << 7U)

static const float Pi = 3.14159265359f;
static const float TwoPi = 6.28318530718f;
static const float FarAway = 1e6f;

struct VSOutput {
	float4 Position : SV_Position;
	float2 Local : TEXCOORD0;		// 半径 = 1 の正規化座標（回転前）。x = ワールド X, y = ワールド Z
};

static const float2 QuadCorners[6] = {
	float2(-1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, -1.0f),
	float2(1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, 1.0f),
};

VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	const float2 corner = QuadCorners[vertexID_ % 6U] * Extent;
	const float3 worldPos = Center + float3(corner.x * Radius, Height, corner.y * Radius);

	VSOutput output;
	output.Position = mul(float4(worldPos, 1.0f), ViewProj);
	output.Local = corner;
	return output;
}

//////	//////	//////	//////	//////	//////
//	Helpers									//
//////	//////	//////	//////	//////	//////

float2 Rotate(float2 p_, float angle_) {
	float s, c;
	sincos(angle_, s, c);
	return float2(c * p_.x - s * p_.y, s * p_.x + c * p_.y);
}

float SdSegment(float2 p_, float2 a_, float2 b_) {
	const float2 pa = p_ - a_;
	const float2 ba = b_ - a_;
	const float h = saturate(dot(pa, ba) / dot(ba, ba));
	return length(pa - ba * h);
}

// lowbias32
uint Hash(uint x_) {
	x_ ^= x_ >> 16U;
	x_ *= 0x7feb352dU;
	x_ ^= x_ >> 15U;
	x_ *= 0x846ca68bU;
	x_ ^= x_ >> 16U;
	return x_;
}

float Hash01(uint x_) {
	return (float)(Hash(x_) & 0x00FFFFFFU) / 16777216.0f;
}

// 多角形の i 番目の頂点（真上 = +Z から反時計回り）
float2 PolygonVertex(uint i_) {
	const float a = TwoPi * (float)i_ / (float)PolygonVertices + Pi * 0.5f;
	return PolygonRadius * float2(cos(a), sin(a));
}

//////	//////	//////	//////	//////	//////
//	Elements（距離を返す）					//
//////	//////	//////	//////	//////	//////

// 星形多角形 {N/K}：頂点 i → i+K を全 i について結ぶ（{6/2} は三角形 2 つ = 六芒星になる）
float SdStarPolygon(float2 p_) {
	float d = FarAway;
	[loop]
	for (uint i = 0U; i < PolygonVertices; ++i) {
		const uint j = (i + PolygonStep) % PolygonVertices;
		d = min(d, SdSegment(p_, PolygonVertex(i), PolygonVertex(j)));
	}
	return d;
}

float SdNodes(float2 p_) {
	float d = FarAway;
	[loop]
	for (uint i = 0U; i < PolygonVertices; ++i) {
		d = min(d, abs(length(p_ - PolygonVertex(i)) - NodeRadius));
	}
	return d;
}

// 等間隔の放射状の線分。accent_ なら 4 本ごとに内側へ倍の長さ
float SdRadialLines(float2 p_, float angle_, uint count_, float inner_, float outer_, bool accent_) {
	const float sector = TwoPi / (float)count_;
	const float idx = round(angle_ / sector);
	const float2 q = Rotate(p_, -idx * sector);

	float inner = inner_;
	if (accent_) {
		const int i = (int)idx;
		if (((i % 4) + 4) % 4 == 0) {
			inner = inner_ - (outer_ - inner_);
		}
	}
	return SdSegment(q, float2(inner, 0.0f), float2(outer_, 0.0f));
}

// 3x3 の格子点を結ぶ 16 本の候補ストローク（gx0, gy0, gx1, gy1）
// gx = 接線方向、gy = 半径方向（外向き）
static const float4 RuneStrokes[16] = {
	// 横
	float4(-1.0f, -1.0f, 0.0f, -1.0f), float4(0.0f, -1.0f, 1.0f, -1.0f),
	float4(-1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f),
	float4(-1.0f, 1.0f, 0.0f, 1.0f), float4(0.0f, 1.0f, 1.0f, 1.0f),
	// 縦
	float4(-1.0f, -1.0f, -1.0f, 0.0f), float4(-1.0f, 0.0f, -1.0f, 1.0f),
	float4(0.0f, -1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f),
	float4(1.0f, -1.0f, 1.0f, 0.0f), float4(1.0f, 0.0f, 1.0f, 1.0f),
	// 斜め（中心から四隅）
	float4(0.0f, 0.0f, -1.0f, -1.0f), float4(0.0f, 0.0f, 1.0f, -1.0f),
	float4(0.0f, 0.0f, -1.0f, 1.0f), float4(0.0f, 0.0f, 1.0f, 1.0f),
};

// ルーン帯：角度方向にセルを切り、セル番号のハッシュでストロークを選ぶ
float SdRunes(float2 p_, float r_, float angle_, out float shimmer_) {
	shimmer_ = 1.0f;

	// 帯から十分離れていれば計算しない（グローが届く範囲だけ）
	const float margin = GlowWidth * 6.0f + LineWidth;
	if (r_ < RuneInner - margin || r_ > RuneOuter + margin) { return FarAway; }

	const float sector = TwoPi / (float)RuneCount;
	const float cellF = min(floor((angle_ + Pi) / sector), (float)RuneCount - 1.0f);
	const uint cell = (uint)cellF;
	const float cellCenter = (cellF + 0.5f) * sector - Pi;

	// q.x = 半径方向, q.y = 接線方向
	const float2 q = Rotate(p_, -cellCenter);

	const float rc = 0.5f * (RuneInner + RuneOuter);
	const float hh = 0.5f * (RuneOuter - RuneInner) * 0.8f;
	const float hw = min(hh, rc * sin(sector * 0.5f) * 0.7f);	// 隣のセルにはみ出さない幅

	const uint seed = Hash(cell * 747796405U + RuneSeed * 2891336453U + 1U);

	uint mask = 0U;
	[unroll]
	for (uint i = 0U; i < 16U; ++i) {
		if (Hash01(seed + i * 0x9E3779B9U) < RuneDensity) { mask |= (1U << i); }
	}
	if (mask == 0U) { mask = 1U << (seed % 16U); }

	float d = FarAway;
	[unroll]
	for (uint k = 0U; k < 16U; ++k) {
		if ((mask & (1U << k)) != 0U) {
			const float4 s = RuneStrokes[k];
			const float2 a = float2(rc + s.y * hh, s.x * hw);
			const float2 b = float2(rc + s.w * hh, s.z * hw);
			d = min(d, SdSegment(q, a, b));
		}
	}

	shimmer_ = 1.0f - RuneShimmer * (0.5f + 0.5f * sin(Time * 3.0f + Hash01(seed) * TwoPi));
	return d;
}

//////	//////	//////	//////	//////	//////
//	Pixel shader							//
//////	//////	//////	//////	//////	//////

// x = 芯, y = グロー
float2 Stroke(float d_, float aa_) {
	const float core = 1.0f - smoothstep(LineWidth - aa_, LineWidth + aa_, d_);
	const float glow = GlowAmount * exp(-max(d_ - LineWidth, 0.0f) / GlowWidth);
	return float2(core, glow);
}

float4 PSMain(VSOutput input_) : SV_TARGET0 {
	const float2 p = Rotate(input_.Local, -Angle);
	const float r = length(p);
	const float angle = atan2(p.y, p.x);	// -π..π

	// 画面上 1px が正規化単位でどれくらいか（分岐の外で取る）
	const float aa = max(fwidth(r), 1e-5f);

	float d = FarAway;
	if ((Elements & ELEMENT_RING0) != 0U) { d = min(d, abs(r - Ring0)); }
	if ((Elements & ELEMENT_RING1) != 0U) { d = min(d, abs(r - Ring1)); }
	if ((Elements & ELEMENT_POLYGON) != 0U) { d = min(d, SdStarPolygon(p)); }
	if ((Elements & ELEMENT_NODES) != 0U) { d = min(d, SdNodes(p)); }
	if ((Elements & ELEMENT_TICKS) != 0U) { d = min(d, SdRadialLines(p, angle, TickCount, TickInner, TickOuter, true)); }
	if ((Elements & ELEMENT_SPOKES) != 0U) { d = min(d, SdRadialLines(p, angle, SpokeCount, SpokeInner, SpokeOuter, false)); }

	float2 lines = Stroke(d, aa);

	if ((Elements & ELEMENT_RUNES) != 0U) {
		float shimmer;
		const float runeD = SdRunes(p, r, angle, shimmer);
		lines += Stroke(runeD, aa) * shimmer;
	}

	float fill = 0.0f;
	if ((Elements & ELEMENT_FILL) != 0U) {
		fill = FillAmount * (1.0f - smoothstep(0.0f, 1.0f, r));
	}

	//----	------	------	------	------	----//
	// 描き込み（真上から時計回り）。Reveal = 1 で全表示
	const float t = frac((Pi * 0.5f - angle) / TwoPi);
	const float reveal =
		(Reveal >= 0.999f) ? 1.0f :
		(Reveal <= 0.001f) ? 0.0f :
		1.0f - smoothstep(Reveal - 0.02f, Reveal, t);

	// クアッドの縁でグローが切れて見えないように
	const float edgeFade = saturate((Extent - r) / 0.1f);

	// 芯は白寄り、グローと照り返しは指定色
	const float3 coreColor = lerp(Color, float3(1.0f, 1.0f, 1.0f), 0.5f);
	const float3 color = (coreColor * lines.x + Color * (lines.y + fill)) * Intensity * reveal * edgeFade;

	return float4(color, 0.0f);
}
