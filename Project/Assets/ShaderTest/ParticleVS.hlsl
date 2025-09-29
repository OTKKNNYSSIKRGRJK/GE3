struct Particle {
	float3 Position;
	float3 Velocity;
	float4 Color;
	uint Duration;
	uint FrameCnt;
};

#define ParticleSizeInByte 48U

StructuredBuffer<Particle> ParticlePool : register(t0);
StructuredBuffer<uint> Indices_Active : register(t1);

struct VSInput {
	float4 LocalPosition : POSITION0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

struct ViewProjection {
	float4x4 Mat;
};
ConstantBuffer<ViewProjection> VP : register(b0);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

void SetVSOutput(inout VSOutput output_, in VSInput input_, uint instID_) {
	uint idx = Indices_Active[instID_];
	output_.Position = input_.LocalPosition + float4(ParticlePool[idx].Position, 0.0f);
	output_.Position = mul(output_.Position, VP.Mat);
	output_.Color = ParticlePool[idx].Color;
	output_.TexCoord = input_.TexCoord;
}

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID) {
	VSOutput output;
	
	SetVSOutput(output, input_, instID_);
	
	return output;
}