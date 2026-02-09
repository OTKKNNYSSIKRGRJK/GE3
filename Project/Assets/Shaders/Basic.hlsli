struct VSOutput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float2 TexCoord : TEXCOORD0;
	nointerpolation uint TexID : TEXID0;
};