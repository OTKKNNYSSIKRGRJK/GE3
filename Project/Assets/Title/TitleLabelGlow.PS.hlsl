//////	//////	//////	//////	//////	//////
//	Title label glow PS						//
//////	//////	//////	//////	//////	//////
// SpriteRenderer（Sprite2.VS.hlsl）用。入力・リソースは SpriteUI.PS.hlsl と同じ。
// ラベル画像の α をぼかして、文字のまわりに光のにじみを作る（加算で描く前提）。
//
// Material.RGBA の意味がこの PS だけ違う：
//   rgb = グローの色 × 強さ（リニア。明滅は CPU 側で掛けておく）
//   a   = にじみの半径（画面上の px）
// グロー用スプライトは「にじむ分だけ大きいクアッド＋ 0..1 の外まで伸ばした UV」で描く。
// 静的サンプラは Wrap なので、0..1 の外は自前で透明として扱う。

struct PSInput {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	uint InstanceID : INSTID0;
};

struct Material {
	float4 RGBA;
	uint TextureID;
};

StructuredBuffer<Material> SRV_Materials : register(t0, space0);

Texture2D<float4> SRV_Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

static const uint NumTaps = 32U;
static const float GoldenAngle = 2.39996323f;

float SampleAlpha(Texture2D<float4> tex_, float2 uv_, float lod_) {
	if (any(uv_ < 0.0f) || any(uv_ > 1.0f)) { return 0.0f; }
	return tex_.SampleLevel(Sampler, uv_, lod_).a;
}

float4 main(PSInput input_) : SV_TARGET0 {
	const Material material = SRV_Materials[input_.InstanceID];
	Texture2D<float4> tex = SRV_Textures[NonUniformResourceIndex(material.TextureID)];

	uint width, height, num_Levels;
	tex.GetDimensions(0U, width, height, num_Levels);

	// 画面 1px あたりの UV から、px 単位の半径を UV に直す（縦横比が違っても円くにじむ）
	const float2 uvPerPixel = fwidth(input_.TexCoord);
	const float2 radiusUV = uvPerPixel * max(material.RGBA.a, 0.0f);

	// タップの間隔に見合う mip から読んで、まばらなタップでもなめらかにする
	const float texelsPerTap = max(radiusUV.x * (float)width, radiusUV.y * (float)height) * 0.25f;
	const float lod = min(log2(max(texelsPerTap, 1.0f)), (float)(num_Levels - 1U));

	// 黄金角スパイラルでディスク状にサンプリング。外側ほど軽い重み
	float sum = 0.0f;
	float weightSum = 0.0f;
	[unroll]
	for (uint i = 0U; i < NumTaps; ++i) {
		const float r = sqrt(((float)i + 0.5f) / (float)NumTaps);
		const float angle = (float)i * GoldenAngle;
		const float2 offset = float2(cos(angle), sin(angle)) * r;
		const float weight = exp(-3.0f * r * r);
		sum += SampleAlpha(tex, input_.TexCoord + offset * radiusUV, lod) * weight;
		weightSum += weight;
	}
	const float glow = sum / weightSum;

	return float4(material.RGBA.rgb * glow, 0.0f);
}
