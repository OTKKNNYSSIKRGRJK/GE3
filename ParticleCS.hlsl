struct Particle {
	float3 Position;
	float3 Velocity;
	float4 Color;
	uint Duration;
	uint FrameCnt;
};

struct EnvironmentVariables {
	float Time;
	float AccelFieldCoefs[9];
};

RWStructuredBuffer<Particle> ParticlePool : register(u0);
AppendStructuredBuffer<uint> In_Indices_Active : register(u1);
AppendStructuredBuffer<uint> In_Indices_Inactive : register(u2);
ConsumeStructuredBuffer<uint> Out_Indices_Inacitve : register(u3);

StructuredBuffer<Particle> ParticleInitDataArray : register(t0);
ConstantBuffer<EnvironmentVariables> EnvVars : register(b0);

#define NUM_THREAD_X 256U

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

[numthreads(NUM_THREAD_X, 1U, 1U)]
void Initialize(uint3 dtid_ : SV_DispatchThreadID) {
	uint idx = dtid_.x;
	
	ParticlePool[idx].Duration = 0U;
	ParticlePool[idx].FrameCnt = 0U;
	In_Indices_Inactive.Append(idx);
}

//////	//////	//////	//////	//////	//////

void InitializeNewParticle(inout Particle p_, in Particle initData_) {
	p_.Position = initData_.Position;
	p_.Velocity = initData_.Velocity;
	p_.Color = initData_.Color;
	p_.Duration = initData_.Duration;
	p_.FrameCnt = 0U;
}

[numthreads(NUM_THREAD_X, 1U, 1U)]
void Emit(uint3 dtid_ : SV_DispatchThreadID) {
	uint idx = dtid_.x;
	uint idx_New = Out_Indices_Inacitve.Consume();
	
	InitializeNewParticle(ParticlePool[idx_New], ParticleInitDataArray[idx]);
}

//////	//////	//////	//////	//////	//////

bool IsAlive(in Particle p_) {
	return p_.FrameCnt < p_.Duration;
}

float3 AccelField(in float3 pos_) {
	return
		0.0001f * pos_ *
		float3(
			EnvVars.AccelFieldCoefs[0] + EnvVars.AccelFieldCoefs[3] + EnvVars.AccelFieldCoefs[6],
			EnvVars.AccelFieldCoefs[1] + EnvVars.AccelFieldCoefs[4] + EnvVars.AccelFieldCoefs[7],
			EnvVars.AccelFieldCoefs[2] + EnvVars.AccelFieldCoefs[5] + EnvVars.AccelFieldCoefs[8]
		);
}

void UpdateParticle(inout Particle p_, uint pid_) {
	if (IsAlive(p_)) {
		p_.Position += p_.Velocity;
		p_.Velocity += AccelField(p_.Position) * cos(float(p_.FrameCnt) * 0.001f);
		p_.Color += float4(-0.0015f, 0.00025f, 0.004f, -0.00006103515625f);
		++p_.FrameCnt;
		
		if (IsAlive(p_)) {
			In_Indices_Active.Append(pid_);
		}
		else{
			In_Indices_Inactive.Append(pid_);
		}
	}
}

[numthreads(NUM_THREAD_X, 1U, 1U)]
void Update(uint3 dtid_ : SV_DispatchThreadID) {
	uint idx = dtid_.x;
	
	UpdateParticle(ParticlePool[idx], idx);
}