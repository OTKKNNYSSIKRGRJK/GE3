struct VSOutput {
	float4 SVPos : SV_Position;
	uint InstanceID : INSTID0;
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
struct DIRECTIONAL_LIGHT {
	float3 Direction;
	LIGHT Diffuse;
	LIGHT Specular;
};
struct POINT_LIGHT {
	float4 WorldPosition;
	LIGHT Diffuse;
	LIGHT Specular;
};
struct SPOT_LIGHT {
	float3 WorldPosition;
	float3 Direction;
	float Distance;
	float Decay;
	float Cos_Angle;
	float Cos_FalloffStart;
	LIGHT Light;
};

ByteAddressBuffer SRV_Lights : register(t0, space1);
void LoadDirectionalLight(inout DIRECTIONAL_LIGHT light_, uint id_) {
	light_.Direction = asfloat(SRV_Lights.Load3(id_ * 44));
	light_.Diffuse.Color = asfloat(SRV_Lights.Load3(id_ * 44 + 12));
	light_.Diffuse.Intensity = asfloat(SRV_Lights.Load(id_ * 44 + 24));
	light_.Specular.Color = asfloat(SRV_Lights.Load3(id_ * 44 + 28));
	light_.Specular.Intensity = asfloat(SRV_Lights.Load(id_ * 44 + 40));
}
void LoadPointLight(inout POINT_LIGHT light_, uint id_) {
	light_.WorldPosition = asfloat(SRV_Lights.Load4(id_ * 48));
	light_.Diffuse.Color = asfloat(SRV_Lights.Load3(id_ * 48 + 16));
	light_.Diffuse.Intensity = asfloat(SRV_Lights.Load(id_ * 48 + 28));
	light_.Specular.Color = asfloat(SRV_Lights.Load3(id_ * 48 + 32));
	light_.Specular.Intensity = asfloat(SRV_Lights.Load(id_ * 48 + 44));
}
//ConstantBuffer<LIGHT> CBV_AmbientLight : register(b0);

cbuffer CBV_Scene : register(b0, space1) {
	float4x4 ScreenToWorld;
	float3 WorldPos_Camera;
	float ModelShininess;
}

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

//float3 CalcSpecular(
//	in LIGHT light_,
//	in float3 viewSpacePos_,
//	in float3 norm_,
//	in float3 lightDir_,
//	float inv_LightDist_
//) {
//	float3 V = -normalize(viewSpacePos_);
//	float3 H = normalize(-lightDir_ + V);
//	float NDotH = saturate(dot(norm_, H));
//	return
//		light_.Color *
//		light_.Intensity *
//		//pow(NDotH, Material.Shininess) *
//		(inv_LightDist_ * inv_LightDist_);
//}

float3 CalcSpecular(
	in LIGHT light_,
	in float3 worldPos_,
	in float3 norm_,
	in float3 lightDir_,
	float inv_LightDist_
) {
	float3 dir_ToEye = normalize(WorldPos_Camera.xyz - worldPos_);
	
	//float3 refl = reflect(lightDir_, norm_);
	//float dot_R_E = saturate(dot(refl, dir_ToEye));
	//return
	//	light_.Color *
	//	light_.Intensity *
	//	pow(dot_R_E, ModelShininess) *
	//	(inv_LightDist_ * inv_LightDist_);
	
	float3 halfVec = normalize(-lightDir_ + dir_ToEye);
	float dot_N_H = saturate(dot(norm_, halfVec));
	return
		light_.Color *
		light_.Intensity *
		pow(dot_N_H, ModelShininess) *
		(inv_LightDist_ * inv_LightDist_);
}

//static float Inv_0xFFFFFF = 1.0f / float(0xFFFFFF);
//static float Inv_0xFF = 1.0f / float(0xFF);
static float2 Inv_WH = float2(1.0f / 1280.0f, 1.0f / 720.0f);

PSOutput CalcPointLight(VSOutput input_) {
	PSOutput output;
	
	float4 albedo = SRV_GBuffer_Albedo.Sample(Sampler_Default, input_.SVPos.xy * Inv_WH);
	float3 normal = SRV_GBuffer_Normal.Sample(Sampler_Default, input_.SVPos.xy * Inv_WH).xyz;
	normal = normal * 2.0f - 1.0f;
	
	
	POINT_LIGHT pointLight;
	LoadPointLight(pointLight, input_.InstanceID);
	
	float depth = SRV_GBuffer_Depth.Load(int3(input_.SVPos.xy, 0.0f));
	float4 worldPos_Target = mul(float4(input_.SVPos.xy, depth, 1.0f), ScreenToWorld);
	worldPos_Target /= worldPos_Target.w;
	
	float3 d_LightToTarget = worldPos_Target.xyz - pointLight.WorldPosition.xyz;
	float lightDist = max(length(d_LightToTarget), 0.0625f);
	float inv_LightDist = 1.0f / lightDist;
	float3 lightDir = d_LightToTarget * inv_LightDist;
	
	float3 light_Diffuse = CalcDiffuse(
		pointLight.Diffuse,
		normal,
		lightDir,
		inv_LightDist
	);
	
	float3 light_Specular = CalcSpecular(
		pointLight.Specular,
		worldPos_Target.xyz,
		normal,
		lightDir,
		inv_LightDist
	);
	
	output.Color = albedo * float4(light_Diffuse + light_Specular, 1.0f);
	
	return output;
}

PSOutput CalcDirectionalLight(VSOutput input_) {
	PSOutput output;
	
	const float4 albedo = SRV_GBuffer_Albedo.Sample(Sampler_Default, input_.SVPos.xy * Inv_WH);
	float3 normal = SRV_GBuffer_Normal.Sample(Sampler_Default, input_.SVPos.xy * Inv_WH).xyz;
	normal = normal * 2.0f - 1.0f;
	
	float depth = SRV_GBuffer_Depth.Load(int3(input_.SVPos.xy, 0.0f));
	
	float4 worldPos_Target = mul(float4(input_.SVPos.xy, depth, 1.0f), ScreenToWorld);
	worldPos_Target /= worldPos_Target.w;
	
	DIRECTIONAL_LIGHT directionalLight;
	LoadDirectionalLight(directionalLight, input_.InstanceID);
	
	float3 light_Diffuse = CalcDiffuse(
		directionalLight.Diffuse,
		normal,
		directionalLight.Direction,
		1.0f
	);
	
	float3 light_Specular = CalcSpecular(
		directionalLight.Specular,
		worldPos_Target.xyz,
		normal,
		directionalLight.Direction,
		1.0f
	);
	
	float3 light = light_Diffuse + light_Specular;
	
	output.Color = albedo * float4(light, 1.0f);
	
	return output;
}