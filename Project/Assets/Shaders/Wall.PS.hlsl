RWTexture2D<float4> Target : register(u0, space0);

Texture2D<float> RippleTexture : register(t0, space1);
Texture2D<float> DepthTexture : register(t1, space1);
SamplerState DefaultSampler : register(s0);

struct ENVVARS {
	float Timer;
};
ConstantBuffer<ENVVARS> EnvVars : register(b0, space0);

struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

static const float2 Inv_WH = float2(1.0f / 1280.0f, 1.0f / 720.0f);
static const float Diff = 1.0f / 64.0f;

void main(VSOutput input_) {
	float2 d = float2(
		(
			RippleTexture.Sample(DefaultSampler, input_.TexCoord + Diff * float2(1.0f, 0.0f)) -
			RippleTexture.Sample(DefaultSampler, input_.TexCoord + Diff * float2(-1.0f, 0.0f))
		) * 0.5f,
		(
			RippleTexture.Sample(DefaultSampler, input_.TexCoord + Diff * float2(0.0f, 1.0f)) -
			RippleTexture.Sample(DefaultSampler, input_.TexCoord + Diff * float2(0.0f, -1.0f))
		) * 0.5f
	);
	
	int depthTest = !!(DepthTexture.Sample(DefaultSampler, input_.Position.xy * Inv_WH) > input_.Position.z);
	d *= depthTest;
	Target[int2(input_.Position.xy)] = Target.Load(int3(input_.Position.xy + d * 5.0f, 0.0f));
	Target[int2(input_.Position.xy)].rgb *= (1.0f - 0.1f * depthTest * clamp(EnvVars.Timer, 0.0f, 1.0f));
	//float4 texColor = Target.Sample(DefaultSampler, input_.TexCoord);
	//output.Color = texColor * input_.Color * 2.0f;
}