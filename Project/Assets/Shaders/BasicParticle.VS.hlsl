struct RENDER_DATA {
	float4x4 Transform;
	float4 RGBA;
	uint TextureID;
	uint AtlasID;
};
StructuredBuffer<RENDER_DATA> Array_RenderData : register(t0);

struct GLOBAL_DATA {
	float4x4 ViewToWorld;
};
ConstantBuffer<GLOBAL_DATA> GlobalData : register(b0, space1);

struct ViewProjection {
	float4x4 Mat;
};
ConstantBuffer<ViewProjection> VP : register(b1, space1);

struct VSInput {
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	nointerpolation uint TexID : TEXID0;
};

#define ATLAS_W		32.0f
#define ATLAS_H		32.0f

namespace Orbits {
	static const float INV_TEX_W = 1.0f / 320.0f;
	static const float INV_TEX_H = 1.0f / 32.0f;
}

VSOutput main(VSInput input_, uint instID_ : SV_InstanceID) {
	VSOutput output;
	
	RENDER_DATA data = Array_RenderData[instID_];
	
	output.Position = mul(input_.Position, data.Transform);
	output.Position = mul(output.Position, VP.Mat);
	
	float w = float(data.AtlasID & 0x07);
	float h = float(data.AtlasID >> 3);
	output.Color = data.RGBA;
	output.TexCoord = (input_.TexCoord + float2(w, h)) * float2(ATLAS_W * Orbits::INV_TEX_W, ATLAS_H * Orbits::INV_TEX_H);
	//output.TexCoord = input_.TexCoord;
	//output.TexCoord *= float2(ATLAS_W * INV_TEX_W, ATLAS_H * INV_TEX_H);
	//output.TexCoord += float2(ATLAS_W * INV_TEX_W * w, ATLAS_H * INV_TEX_H * h);
	output.TexID = data.TextureID;
	
	return output;
}