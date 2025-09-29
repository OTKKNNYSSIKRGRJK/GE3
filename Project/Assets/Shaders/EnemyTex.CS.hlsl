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

static const float3 a = { 0.95f, 0.65f, 0.45f };
static const float3 b = { 0.15f, 0.3f, 0.15f };

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

[numthreads(16U, 16U, 1U)]
void main(uint2 dtid_ : SV_DispatchThreadID) {
	float2 UV = (float2(dtid_) + offset) * inv_WH;
	
	float Len_UV0 = length(UV);
	
	UV = mul(UV, Rotate2D(atan2(UV.y, UV.x) * 1.0f));
	
	float ww = cos(TIME * 2.0f) * 0.5f + cos(TIME * 1.6f) * 0.3f + cos(TIME * 3.75f) * 0.2f;
	
	float3 color = { 0.0f, 0.0f, 0.0f };
	
	for (float i = 0.0f; i < 3.0f; i += 1.0f) {
		UV = mul(UV, Rotate2D(-1.0f * (frac(i * 0.5f) * 2.0f) * atan2(UV.y, UV.x)));
		UV = frac(UV * 3.0f) - 0.5f;
		float2 UV2 = cos(UV) * ww + 3.0f;
		
		float d = length(UV2) * exp(-cos(TIME * 0.4f) * 0.25f - length(UV) * 2.0f - Len_UV0);
		d = sin(d * 4.0f + TIME * (sin(TIME * 0.25f) * 0.05f + 1.0f));
		d = (1.0f - abs(d));
		d *= cos(atan2(UV.y, UV.x) * 8.0f) + sin(TIME * 2.16) * 0.5;
		d *= d;
		//if(cos(atan(fragPosition.y-Resolution.y*0.5,fragPosition.x-Resolution.x*0.5)*8.0+Timer)>0) a *= vec3(0.8, 0.8, 1.0);
		
		color += Palette(a, b, Len_UV0 + i * 0.5f + TIME * 0.5f) * d;
	}
	
	Output[dtid_] = float4(color, color.b);
}