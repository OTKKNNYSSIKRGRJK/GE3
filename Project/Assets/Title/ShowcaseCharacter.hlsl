//////	//////	//////	//////	//////	//////
//	ShowcaseCharacter (prototype)			//
//////	//////	//////	//////	//////	//////
// 行優先（-Zpr）・行ベクトル（mul(v, M)）前提
// VSStatic : 武器などの静的メッシュ用（スキンメッシュは Assets/Shaders/MeshSkinning.VS.hlsl を流用）
// PSMain   : 炎の照り返し（セル調）＋半球環境光＋リム＋自己発光 → SRGB RT へ不透明出力

// MeshSkinning.VS.hlsl の cbuffer Global と同じレジスタ・並び
cbuffer Transforms : register(b0, space16) {
	float4x4 WVP;
	float4x4 World;
	float4x4 TR_INV_World;
};

// 色はすべて CPU 側でリニア化＆強度を掛け済み
cbuffer Shading : register(b0, space0) {
	float3 BaseColor;		float CelThreshold;
	float3 EmissiveColor;	float CelSoftness;
	float3 FireBase;		float FireHeight;
	float3 FireColor;		float UnderWeight;
	float3 SkyColor;		float ShadowLevel;
	float3 GroundColor;		float RimThreshold;
	float3 RimColor;		float RimSoftness;
	float3 CameraPos;		float Padding0;
};

struct VSInputStatic {
	float3 Pos : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

// MeshSkinning.VS.hlsl の VSOutput と同じ並び（PS を共有するため）
struct VSOutput {
	float4 Pos : SV_Position;
	float3 WorldPos : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

VSOutput VSStatic(VSInputStatic input_) {
	const float4 pos = float4(input_.Pos, 1.0f);

	VSOutput output;
	output.Pos = mul(pos, WVP);
	output.WorldPos = mul(pos, World).xyz;
	output.TexCoord = input_.TexCoord;
	output.Normal = mul(input_.Normal, (float3x3)TR_INV_World);
	return output;
}

float4 PSMain(VSOutput input_) : SV_TARGET0 {
	const float3 N = normalize(input_.Normal);
	const float3 V = normalize(CameraPos - input_.WorldPos);

	//----	------	------	------	------	----//
	// 炎の照り返し
	// キャラは火柱の内側にいるので、光は「軸から外向き」かつ「下（根元）」から来る扱いにする
	const float3 fromAxis = input_.WorldPos - FireBase;
	const float radialLength = length(fromAxis.xz);
	const float2 radial = (radialLength > 1e-4f) ? (fromAxis.xz / radialLength) : float2(0.0f, 0.0f);
	const float3 L = normalize(float3(radial.x, -UnderWeight - 1e-3f, radial.y));

	// 根元ほど強く、FireHeight で 0
	const float h = saturate(fromAxis.y / FireHeight);
	const float atten = (1.0f - h) * (1.0f - h);

	// セル調の 2 段階（ShadowLevel で影側の残り具合）
	const float band = smoothstep(CelThreshold - CelSoftness, CelThreshold + CelSoftness, dot(N, L));
	const float3 fire = FireColor * atten * lerp(ShadowLevel, 1.0f, band);

	//----	------	------	------	------	----//
	// 半球環境光（下を向く面ほど地面＝魔法陣・炎の色）
	const float3 ambient = lerp(GroundColor, SkyColor, N.y * 0.5f + 0.5f);

	//----	------	------	------	------	----//
	// リム（輪郭の縁取り）。上の方はやや弱める
	const float rimRaw = 1.0f - saturate(dot(N, V));
	const float rim = smoothstep(RimThreshold - RimSoftness, RimThreshold + RimSoftness, rimRaw);
	const float3 rimLight = RimColor * rim * lerp(0.35f, 1.0f, atten);

	const float3 color = BaseColor * (ambient + fire) + rimLight + EmissiveColor;
	return float4(color, 1.0f);
}
