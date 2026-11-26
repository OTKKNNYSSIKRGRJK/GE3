//////	//////	//////	//////	//////	//////
//	SceneLight - 平行光 1 本＋環境光		//
//////	//////	//////	//////	//////	//////
// InGame のディファードは点光源（粒子）しか足さないので、粒子の光が届かない所は真っ黒になる。
// その暗さを和らげるための全画面パス（2026-11-11）。MergePass で、ライティングの結果を写した直後に加算で描く。
//
//   色 = アルベド × AlbedoScale × (平行光の色 × 強さ × 回り込み付きの N・L ＋ 半球の環境光)
//   空（深度 = 1、スカイボックスは xyww で 1 になる）は、アルベド × SkyBrightness をそのまま足す
//
// 影は無い（シャドウマップが無いので、平行光は物を突き抜ける）。
// G-buffer: t0 = アルベド（SRGB。読むとリニア）、t1 = 法線（ワールド、×0.5+0.5 で入っている）、t2 = 深度

Texture2D<float4> GBufferAlbedo : register(t0, space0);
Texture2D<float4> GBufferNormal : register(t1, space0);
Texture2D<float> GBufferDepth : register(t2, space0);

struct LIGHT {
	float4 ToLightIntensity;	// xyz = 光の来る向き（長さ 1）、w = 強さ
	float4 ColorWrap;			// rgb = 色（リニア）、w = 回り込み（0 = ランバート）
	float4 SkyAmbient;			// rgb = 上からの環境光の色（リニア）、w = 環境光の強さ
	float4 GroundAmbient;		// rgb = 下からの環境光の色（リニア）、w = 空の明るさ
	float4 Misc;				// x = アルベドに掛ける値（点光源と同じ 0.75）
};
ConstantBuffer<LIGHT> Light : register(b0, space0);

struct VSOutput {
	float4 Position : SV_Position;
};

// 画面を覆う大きな三角形 1 枚
VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	float2 uv = float2((vertexID_ << 1) & 2, vertexID_ & 2);
	VSOutput output;
	output.Position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return output;
}

float4 PSMain(VSOutput input_) : SV_Target0 {
	int3 pix = int3(input_.Position.xy, 0);
	float3 albedo = GBufferAlbedo.Load(pix).rgb;
	float depth = GBufferDepth.Load(pix);

	if (depth >= 0.99999f) {
		return float4(albedo * Light.GroundAmbient.w, 0.0f);
	}

	float3 n = GBufferNormal.Load(pix).xyz * 2.0f - 1.0f;
	float len = length(n);
	n = (len > 0.01f) ? (n / len) : float3(0.0f, 1.0f, 0.0f);

	float wrap = saturate(Light.ColorWrap.w);
	float ndl = dot(n, Light.ToLightIntensity.xyz);
	float diffuse = saturate((ndl + wrap) / (1.0f + wrap));
	float3 direct = Light.ColorWrap.rgb * Light.ToLightIntensity.w * diffuse;
	float3 ambient = lerp(Light.GroundAmbient.rgb, Light.SkyAmbient.rgb, n.y * 0.5f + 0.5f) * Light.SkyAmbient.w;

	return float4(max(albedo * Light.Misc.x * (direct + ambient), 0.0f), 0.0f);
}
