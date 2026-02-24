struct GLOBAL_DATA {
	float4x4 ViewToWorld;
};

ConstantBuffer<GLOBAL_DATA> GlobalData : register(b0, space1);

struct PSInput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	uint TexID : TEXID0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> Textures[] : register(t0, space1);
Texture2D<float> SRV_DepthTexture : register(t2, space2);
SamplerState Sampler : register(s0);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

PSOutput main(in PSInput input_) {
	PSOutput output;
	
	float4 texColor = Textures[input_.TexID].Sample(Sampler, input_.TexCoord);
	//output.Color = texColor * input_.Color;
	output.Color = input_.Color * texColor;
	//output.Color = float4(input_.Position.xy * float2(1.0f / 1280.0f, 1.0f / 720.0f) + 0.5f, 1.0f, 0.75f);
	float depth = SRV_DepthTexture.Load(int3(input_.Position.xy, 0.0f));
	output.Color.w = depth < input_.Position.z ? output.Color.w * 0.1f : output.Color.w;
	return output;
}