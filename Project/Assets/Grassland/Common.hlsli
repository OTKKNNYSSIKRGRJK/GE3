namespace GrassBlade {
	typedef struct {
		float3 LocalPos : POSITION0;
		float2 TexCoord : TEXCOORD0;
		float3 Normal : NORMAL0;
		float3 Tangent : TANGENT0;
	} VSInput;
	
	typedef struct {
		float4 Position : SV_Position;
		float2 TexCoord : TEXCOORD0;
		float2 TexCoord_Grassland : TEXCOORD1;
		float3 Normal : NORMAL0;
		float3 Tangent : TANGENT0;
	} VSOutput, PSInput;
	
	typedef struct {
		float4 Diffuse : SV_Target0;
		float4 Normal : SV_Target1;
	} PSOutput;
}

#define SLOT_SAV_MAP_BEND register(t0)
#define SLOT_SAV_MAP_TRAMPLING register(t1)
#define SLOT_SAV_MAP_NOISE register(t2)
#define SLOT_SRV_MAP_TINT register(t3)

#define SPACE_IMGTEX space1