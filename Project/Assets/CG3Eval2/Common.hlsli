namespace CG3 {
	typedef struct {
		float3 LocalPos : POSITION0;
		float2 TexCoord : TEXCOORD0;
		float TexID : TEXID0;
		float3 Normal : NORMAL0;
	} VSInput;
	
	typedef struct {
		float4 Position : SV_Position;
		float2 TexCoord : TEXCOORD0;
		float TexID : TEXID0;
		float3 Normal : NORMAL0;
	} VSOutput, PSInput;

	typedef struct {
		float4 Albedo : SV_Target0;
		float4 Normal : SV_Target1;
	} PSOutput;
}

#define SPACE_IMGTEX space1