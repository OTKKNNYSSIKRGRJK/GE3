struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

cbuffer Constants0 : register(b0) {
	float Time;
}

Texture2D<float4> OffscreenTexture : register(t0);
SamplerState BilinearClamp : register(s0);

float Random(float3 val_) {
	//make value smaller to avoid artefacts
	float3 smallValue = sin(val_);
	//get scalar value from 3d vector
	float rnd = dot(smallValue, float3(12.9898f, 78.233f, 37.719f));
	//make value more random by making it bigger and then taking teh factional part
	rnd = frac(sin(rnd) * 143758.5453f);
	return rnd;
}

PSOutput main(VSOutput input_) {
	float4 texColor = OffscreenTexture.Sample(BilinearClamp, input_.TexCoord);
	float rnd = Random(float3(input_.Position.xy, Time));
	
	PSOutput output;
	
	output.Color = float4(texColor.rgb, 1.0f);
	output.Color.rgb *= rnd;
	
	return output;
}