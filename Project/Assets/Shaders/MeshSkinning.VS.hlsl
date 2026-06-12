struct Well {
	float4x4 SkeletonSpace;
	float4x4 TR_INV_SkeletonSpace;
};

StructuredBuffer<Well> MatrixPalette : register(t0);

cbuffer Global : register(b0, space16) {
	float4x4 WVP;
	float4x4 World;
	float4x4 TR_INV_World;
}

struct VSInput {
	float3 Pos : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
	float4 Weight : WEIGHT0;
	int4 Palette : PALETTE0;
};

struct VSOutput {
	float4 Pos : SV_Position;
	float3 WorldPos : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 Normal : NORMAL0;
};

struct Skinned {
	float4 Pos;
	float3 Normal;
};

Skinned Skinning(in VSInput in_) {
	Skinned out_;
	
	// p = sum(w_i * v * T_i)
	
	float4 pos = float4(in_.Pos, 1.0f);
	
	out_.Pos = mul(pos, MatrixPalette[in_.Palette.x].SkeletonSpace) * in_.Weight.x;
	out_.Pos += mul(pos, MatrixPalette[in_.Palette.y].SkeletonSpace) * in_.Weight.y;
	out_.Pos += mul(pos, MatrixPalette[in_.Palette.z].SkeletonSpace) * in_.Weight.z;
	out_.Pos += mul(pos, MatrixPalette[in_.Palette.w].SkeletonSpace) * in_.Weight.w;
	out_.Pos.w = 1.0f;
	
	out_.Normal = mul(in_.Normal, (float3x3) MatrixPalette[in_.Palette.x].TR_INV_SkeletonSpace) * in_.Weight.x;
	out_.Normal += mul(in_.Normal, (float3x3) MatrixPalette[in_.Palette.y].TR_INV_SkeletonSpace) * in_.Weight.y;
	out_.Normal += mul(in_.Normal, (float3x3) MatrixPalette[in_.Palette.z].TR_INV_SkeletonSpace) * in_.Weight.z;
	out_.Normal += mul(in_.Normal, (float3x3) MatrixPalette[in_.Palette.w].TR_INV_SkeletonSpace) * in_.Weight.w;
	out_.Normal = normalize(out_.Normal);
	
	return out_;
}

void ApplySkinning(out VSOutput vsOUT_, in VSInput vsIN_) {
	Skinned skinned = Skinning(vsIN_);
	vsOUT_.Pos = mul(skinned.Pos, WVP);
	vsOUT_.WorldPos = mul(skinned.Pos, World).xyz;
	vsOUT_.Normal = mul(skinned.Normal, (float3x3) TR_INV_World);
}

VSOutput main(in VSInput vsIN_) {
	VSOutput vsOUT;
	
	ApplySkinning(vsOUT, vsIN_);
	//vsOUT.Pos = mul(float4(vsIN_.Pos, 1.0f), WVP);
	//vsOUT.WorldPos = vsOUT.Pos;
	//vsOUT.Normal = mul(vsIN_.Normal, (float3x3) TR_INV_World);
	
	vsOUT.TexCoord = vsIN_.TexCoord;
	
	return vsOUT;
}