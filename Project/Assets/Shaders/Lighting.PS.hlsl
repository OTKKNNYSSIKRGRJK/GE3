struct VSOutput {
	float4 Position : SV_Position;
	uint LightID : ID0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> SRV_GBuffer_Albedo : register(t0, space16);
Texture2D<float4> SRV_GBuffer_Normal : register(t1, space16);
Texture2D<float> SRV_GBuffer_Depth : register(t2, space16);

SamplerState Sampler_Default : register(s0);

struct LIGHT {
	float3 Color;
	float Intensity;
};
struct POINT_LIGHT {
	float4 WorldPosition;
	LIGHT Light;
};

struct Mat4 {
	float4x4 Mat;
};

StructuredBuffer<POINT_LIGHT> SRV_Arr_PointLight : register(t0, space1);
ConstantBuffer<Mat4> CBV_Matrix_ScreenToWorld : register(b0, space1);

float3 CalcDiffuse(
	in LIGHT light_,
	in float3 norm_,
	in float3 lightDir_,
	float inv_LightSrcDist_
) {
	float NDotL = saturate(dot(norm_, -lightDir_));
	return
		light_.Color *
		light_.Intensity *
		NDotL *
		(inv_LightSrcDist_ * inv_LightSrcDist_);
}

float3 CalcSpecular(
	in LIGHT light_,
	in float3 viewSpacePos_,
	in float3 norm_,
	in float3 lightDir_,
	float inv_LightDist_
) {
	float3 V = -normalize(viewSpacePos_);
	float3 H = normalize(-lightDir_ + V);
	float NDotH = saturate(dot(norm_, H));
	return
		light_.Color *
		light_.Intensity *
		//pow(NDotH, Material.Shininess) *
		(inv_LightDist_ * inv_LightDist_);
}

static float2 Inv_WH = float2(1.0f / 1280.0f, 1.0f / 720.0f);

PSOutput main(VSOutput input_) {
	PSOutput output;
	
	float4 albedo = SRV_GBuffer_Albedo.Sample(Sampler_Default, input_.Position.xy * Inv_WH);
	float3 normal = SRV_GBuffer_Normal.Sample(Sampler_Default, input_.Position.xy * Inv_WH).xyz;
	float depth = SRV_GBuffer_Depth.Load(int3(input_.Position.xy, 0.0f));
	
	POINT_LIGHT pointLight = SRV_Arr_PointLight[input_.LightID];
	
	float4 worldPos_Target = mul(float4(input_.Position.xy, depth, 1.0f), CBV_Matrix_ScreenToWorld.Mat);
	worldPos_Target /= worldPos_Target.w;
	
	float3 d_LightToTarget = worldPos_Target.xyz - pointLight.WorldPosition.xyz;
	float lightDist = max(length(d_LightToTarget), 0.0625f);
	float inv_LightDist = 1.0f / lightDist;
	float3 lightDir = d_LightToTarget * inv_LightDist;
	
	float3 diffuseColor = CalcDiffuse(
		pointLight.Light,
		normal * 2.0f - 1.0f,
		lightDir,
		inv_LightDist
	);
	
	output.Color = albedo * 0.75f * float4(diffuseColor, 1.0f);
	
	return output;
}