//////	//////	//////	//////	//////	//////
//	Title particle PS						//
//////	//////	//////	//////	//////	//////
// Assets/Shaders/BasicParticle.PS.hlsl から、G-buffer の深度テクスチャによる手動遮蔽を抜いたもの。
// タイトルでは PSO の深度テスト（書き込みなし）で遮蔽する。VS は BasicParticle.VS.hlsl をそのまま使う。

struct PSInput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	uint TexID : TEXID0;
};

Texture2D<float4> Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

float4 main(PSInput input_) : SV_TARGET0 {
	const float4 texColor = Textures[input_.TexID].Sample(Sampler, input_.TexCoord);
	return input_.Color * texColor;
}
