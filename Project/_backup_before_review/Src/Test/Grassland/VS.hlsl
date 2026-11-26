#include"Common.hlsli"

cbuffer Scene : register(b1) {
	float4x4 WorldToNDC;
};

Texture2D<float4> SRV_Map_Bend : SLOT_SAV_MAP_BEND;
Texture2D<float4> SRV_Map_Trampling : SLOT_SAV_MAP_TRAMPLING;
Texture2D<float4> SRV_Map_Noise : SLOT_SAV_MAP_NOISE;

SamplerState BilinearWrap : register(s0);

static const uint Mask = 0x1FFU;
static const uint MaskBit = 9U;
static const float Inv_Norm = 1.0f / float(1U << MaskBit);
static const float2 Spacing = float2(0.15f, 0.15f);
static const float2 OffsetMin = (Spacing * Mask * (-0.5f));

float2 OffsetXZ(uint instID_) {
	return float2((instID_ & Mask), (instID_ >> MaskBit)) * Spacing + OffsetMin;
}

float2 MapUV(uint instID_) {
	return float2((instID_ & Mask), (instID_ >> MaskBit)) * Inv_Norm;
}

float3 BezierCurve(
	float t_,
	in float3 p0_,
	in float3 p1_,
	in float3 p2_,
	in float3 p3_
) {
	const float s = 1.0f - t_;
	return
		p0_ * (s * s * s) +
		p1_ * (s * s * t_) +
		p2_ * (s * t_ * t_) +
		p3_ * (t_ * t_ * t_);
}

float2 Diff_BezierCurve(
	float t_,
	in float3 p0_,
	in float3 p1_,
	in float3 p2_,
	in float3 p3_
) {
	const float s = 1.0f - t_;
	return
		p0_ * (-3.0f * s * s) +
		p1_ * (-2.0f * s * t_ + s * s) +
		p2_ * (-1.0f * s * t_ * t_ + 2.0f * s * t_) +
		p3_ * (3.0f * t_ * t_);
}

float3x3 RotateAround(in float3 axis_, float theta_) {
	const float cos_Theta = cos(theta_);
	const float sin_Theta = sin(theta_);

	return float3x3(
		axis_.x * axis_.x * (1.0f - cos_Theta) + cos_Theta,
		axis_.x * axis_.y * (1.0f - cos_Theta) + axis_.z * sin_Theta,
		axis_.x * axis_.z * (1.0f - cos_Theta) - axis_.y * sin_Theta,

		axis_.y * axis_.x * (1.0f - cos_Theta) - axis_.z * sin_Theta,
		axis_.y * axis_.y * (1.0f - cos_Theta) + cos_Theta,
		axis_.y * axis_.z * (1.0f - cos_Theta) + axis_.x * sin_Theta,

		axis_.z * axis_.x * (1.0f - cos_Theta) + axis_.y * sin_Theta,
		axis_.z * axis_.y * (1.0f - cos_Theta) - axis_.x * sin_Theta,
		axis_.z * axis_.z * (1.0f - cos_Theta) + cos_Theta
	);
}

float3x3 RotateAround(in float3 axis_, float cos_Theta_, float sin_Theta_) {
	return float3x3(
		axis_.x * axis_.x * (1.0f - cos_Theta_) + cos_Theta_,
		axis_.x * axis_.y * (1.0f - cos_Theta_) + axis_.z * sin_Theta_,
		axis_.x * axis_.z * (1.0f - cos_Theta_) - axis_.y * sin_Theta_,

		axis_.y * axis_.x * (1.0f - cos_Theta_) - axis_.z * sin_Theta_,
		axis_.y * axis_.y * (1.0f - cos_Theta_) + cos_Theta_,
		axis_.y * axis_.z * (1.0f - cos_Theta_) + axis_.x * sin_Theta_,

		axis_.z * axis_.x * (1.0f - cos_Theta_) + axis_.y * sin_Theta_,
		axis_.z * axis_.y * (1.0f - cos_Theta_) - axis_.x * sin_Theta_,
		axis_.z * axis_.z * (1.0f - cos_Theta_) + cos_Theta_
	);
}

GrassBlade::VSOutput main(GrassBlade::VSInput input_, uint instID_ : SV_InstanceID) {
	GrassBlade::VSOutput output;
	
	const float2 mapUV = MapUV(instID_);
	const float4 pn = SRV_Map_Noise.SampleLevel(BilinearWrap, mapUV, 0.0f);
	const float4 args_Bend = SRV_Map_Bend.SampleLevel(BilinearWrap, mapUV, 0.0f);
	const float4 args_Trampling = SRV_Map_Trampling.SampleLevel(BilinearWrap, mapUV, 0.0f);
	
	output.Position = float4(input_.LocalPos, 1.0f);
	
	output.Position.xyz *= (pn.xyz - 0.5f) * 0.8f + 1.0f;
	
	const float cos_Theta = cos(pn.w * 2.0f * 3.14159265f + pn.z);
	const float sin_Theta = sin(pn.w * 2.0f * 3.14159265f + pn.z);
	const float2x2 rotMat_Blade = float2x2(
		cos_Theta, sin_Theta,
		-sin_Theta, cos_Theta
	);
	output.Position.xz = mul(output.Position.xz, rotMat_Blade);
	
	output.Position.xz += float2(
		(pn.x * pn.z - 0.5f) * 5.0f,
		(pn.y * pn.w - 0.5f) * 5.0f
	);
	output.Position.xz += OffsetXZ(instID_);
	
	const float3 p3 = args_Bend.xyz * args_Trampling.w + args_Trampling.xyz;
	const float3 p2 = p3 * 0.75f + float3(0.0f, 0.25f, 0.0f);
	const float3 p1 = p3 * float3(args_Bend.w, 1.0f, args_Bend.w);
	const float3 p0 = float3(0.0f, 0.0f, 0.0f);
	const float3 bend = BezierCurve(input_.LocalPos.y, p0, p1, p2, p3);
	output.Position.xyz += bend;
	
	output.Position = mul(output.Position, WorldToNDC);
	
	output.TexCoord = input_.TexCoord;
	output.TexCoord_Grassland = mapUV;
	
	const float diff_Bend = Diff_BezierCurve(input_.LocalPos.y, p0, p1, p2, p3);
	float dotProd_Up_Diff_Bend = dot(float3(0.0f, 1.0f, 0.0f), diff_Bend);
	float3 crossProd_Up_Diff_Bend = cross(float3(0.0f, 1.0f, 0.0f), diff_Bend);
	float3x3 rotMat_NormalTangent = RotateAround(
		normalize(crossProd_Up_Diff_Bend),
		dotProd_Up_Diff_Bend,
		length(crossProd_Up_Diff_Bend)
	);
	
	output.Normal = input_.Normal;
	output.Normal.xz = mul(output.Normal.xz, rotMat_Blade);
	output.Normal = mul(output.Normal, rotMat_NormalTangent);
	output.Normal = normalize(output.Normal);
	
	output.Tangent = input_.Tangent;
	
	return output;
}