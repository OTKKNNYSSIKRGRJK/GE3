struct Params {
	float Time;
};
ConstantBuffer<Params> ParamData : register(b0);

#define TIME ParamData.Time

RWTexture2D<float4> Output : register(u0);

float3 Palette(in float3 a, in float3 b, float t) {
	float3 c = float3(1.0f, 1.0f, 1.0f);
	float3 d = float3(0.263f, 0.416, 0.557f);
	
	return a + b * cos(6.28318f * (c * t + d));
}

float2x2 Rotate2D(float t) {
	return float2x2(
		cos(t), sin(t),
		-sin(t), cos(t)
	);
}

float Box(float3 p_, float3 b_) {
	float3 d = abs(p_) - b_;
	return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

static const float PI = acos(-1.0f);
static const float2 offset = { -32.0f, -32.0f };
static const float2 inv_WH = { 1.0f / 64.0f, 1.0f / 64.0f };

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

[numthreads(16U, 16U, 1U)]
void main(uint2 dtid_ : SV_DispatchThreadID) {
	float2 UV = (float2(dtid_) + offset) * inv_WH * (sin(TIME * 2.5f) * .1f + .5f);
	
	float Len_UV0 = length(UV);
	
	float3 a1 = float3(0.025f, 0.5f, 0.6f);
	float3 b1 = float3(0.025f, 0.1f, 0.05f);
	float3 a2 = float3(0.7f, 0.5f, 0.2f);
	float3 b2 = float3(0.1f, 0.2f, 0.1f);
	
	UV = mul(UV, Rotate2D(sin(Len_UV0 * 32.0f) / (Len_UV0 + 0.125f) * 0.0625f));
	UV *= 1.5f + cos(atan2(UV.y, UV.x) * 6.0f) * 0.01f + cos(TIME * 0.75f) * 0.05f;
	UV *= 1.5f + cos(atan2(UV.y, UV.x) * 16.0f) * 0.05f;
	
	float3 color = { 0.0f, 0.0f, 0.0f };
	
	for (float i = 0.0f; i < 3.0f; i += 1.0f) {
		float2 UV2 = UV + float2(6.0f + sin(TIME * 0.5f) * 0.25f, -3.0f + 3.0f * i);
		UV2 *= i * 0.75f + 0.5f;
		
		float d = 2.0f * length(UV2);
		d = sin(d * (0.5f * sin(TIME * 0.1f) + 2.5f) - TIME * 2.0f) * 0.125f;
		d = abs(d) + 0.01f;
		d = 0.01f / d;
		
		color += Palette(a2, b2, Len_UV0 + i * 0.4f + TIME * 0.125f) * d;
	}
	
	Output[dtid_] = float4(color, 1.0f);
}