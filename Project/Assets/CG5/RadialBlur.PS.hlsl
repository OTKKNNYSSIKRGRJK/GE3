struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

cbuffer Constants0 : register(b0) {
	float2 ScreenSpaceCenter;
	float2 RCP_Size;
	float BlurWidth_R;
	float BlurWidth_G;
	float BlurWidth_B;
	uint NUM_Samples;
	float RCP_NUM_Samples;
	float Time;
}

Texture2D<float4> OffscreenTexture : register(t0);
SamplerState BilinearClamp : register(s0);

PSOutput main(VSOutput input_) {
	const float2 dir = (input_.Position.xy - ScreenSpaceCenter) * RCP_Size * 0.1f;
	const float theta = atan2(dir.y, dir.x);
	float3 angleFactor = float3(
		sin(theta * 8.0f + Time * 1.0f),
		sin(theta * 12.0f + Time * 1.5f),
		sin(theta * 16.0f + Time * 2.0f)
	) * 0.1f;
	angleFactor += 0.9f;
	
	float3 blurColor = float3(0.0f, 0.0f, 0.0f);
	for (uint i = 0; i < NUM_Samples; ++i) {
		const float2 uv_R = input_.TexCoord + dir * BlurWidth_R * float(i) * angleFactor.r;
		blurColor.r += OffscreenTexture.Sample(BilinearClamp, uv_R).r;
		const float2 uv_G = input_.TexCoord + dir * BlurWidth_G * float(i) * angleFactor.g;
		blurColor.g += OffscreenTexture.Sample(BilinearClamp, uv_G).g;
		const float2 uv_B = input_.TexCoord + dir * BlurWidth_B * float(i) * angleFactor.b;
		blurColor.b += OffscreenTexture.Sample(BilinearClamp, uv_B).b;
	}
	
	blurColor *= RCP_NUM_Samples;
	
	PSOutput output;
	
	output.Color = float4(blurColor, 1.0f);
	
	return output;
}