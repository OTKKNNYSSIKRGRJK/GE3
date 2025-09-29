struct Params {
	float Time;
};
ConstantBuffer<Params> ParamData : register(b0);

#define TIME ParamData.Time

RWTexture2D<float4> Output : register(u0);

float3 Palette(float t) {
	float3 a = float3(0.5f, 0.5f, 0.7f);
	float3 b = float3(0.1f, 0.3f, 0.3f);
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
	float2 UV = (float2(dtid_) + offset) * inv_WH * (sin(TIME * 2.5f) * .1f + .5f) * .125f;
	
	float Len_UV0 = length(UV);
	//UV *= Len_UV0;
	//float cosTheta = cos(UV.x / Len_UV0);
	//float sinTheta = sin(UV.y / Len_UV0);
	//UV = float2(cosTheta, sinTheta);
	
	//float3 color = float3(
	//	Box(float3(UV, 1.0f), float3(1.0f + sin(Time * 7.0f), 0.5f, 1.5f)),
	//	Box(float3(UV, 1.0f), float3(.3f, 1.5f + sin(Time * 12.0f), 2.5f)),
	//	Box(float3(UV, 1.0f), float3(.4f, 2.5f + sin(Time * 5.0f), 1.5f))
	//);
	
	float3 color = float3(0.05f, 0.1f, 0.35f);
	
	//UV = mul(UV, Rotate2D(sin(TIME) * sin(Len_UV0 * 5.0f) * .5f));
	
	//UV = mul(UV, Rotate2D(atan2(UV.y, UV.x) * 3.0f + TIME));
	UV = mul(UV, Rotate2D(atan2(UV.y, UV.x) * 1.0f));
	
	for (float i = 0.0f; i < 2.0f; i += 1.0f) {
		UV = mul(UV, Rotate2D(atan2(UV.y, UV.x) * 0.5f));
		//UV = mul(UV, Rotate2D(cos(length(UV) * 5.0f + TIME) * 0.5f));
		UV = frac(UV * 7.0f) - 0.5f;
		
		//float d = length(UV) * exp(-Len_UV0 + sin(Time * 2.0f) * 0.125f + 0.25f);
		float d = Box(float3(UV, 1.0f), float3(.15f, .15f, 0.75f)) * exp(-Len_UV0 * 1.5f);
		d += Box(float3(UV * .75f, 1.0f), float3(.3f, .3f, 0.75f)) * exp(-Len_UV0 * 2.5f);
		//float d = length(UV) + Box(float3(UV, 0.0f), float3(abs(cos(Time)) * 0.6f, abs(sin(TIME)) * 0.6f, 0.5f));
		//float d = (1.0f / (abs(cos(UV.y) - cos(UV.x)) + 1.0f) +0.5f) * length(UV);
		//d += abs(cos(atan2(UV.y, UV.x)))*length(UV);
		d = sin(d * 16.0f + TIME * 3.5f) * 0.1f;
		d = abs(d) + 0.001f;
		d = pow(0.01f / d, 1.5f);
		//d = pow(float3(1.0f, 1.0f, 1.0f) - pow(d, 0.2f), 2.0f);
		
		//color += float3(1.0f, 1.0f, 1.0f)*d;
		color += Palette(Len_UV0 + i * 2.5f + TIME * 1.1f) * d;
	}
	
	Output[dtid_] = float4(color, color.b);
}