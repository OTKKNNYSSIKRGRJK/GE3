struct PSInput {
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float3 Normal : NORMAL0;
	float2 TexCoord : TEXCOORD0;
	uint IsWater : FLAG0;
};

struct PSOutput {
	float4 Color : SV_TARGET0;
};

Texture2D<float4> Textures[] : register(t0, space1);
SamplerState Sampler : register(s0);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

struct DIRECTIONAL_LIGHT {
	float4 Color;
	float3 Dir;
};

static DIRECTIONAL_LIGHT DirectionalLight = {
	float4(1.0f, 1.0f, 1.0f, 1.0f),
	normalize(float3(0.0f, -2.0f, 5.0f))
};

ConstantBuffer<DIRECTIONAL_LIGHT> test[] : register(b0, space2);

float4 CalcHalfLambertianReflectance(in PSInput input_) {
	float nDotL = saturate(dot(normalize(input_.Normal.xzy), -DirectionalLight.Dir));
	return
		DirectionalLight.Color *
		pow(nDotL * 0.75f + 0.25f, 2.0f);
}

PSOutput main(in PSInput input_) {
	PSOutput output;
	
	uint texID = input_.IsWater ? 3 : 2;
	//Texture2D<float4> tex = ResourceDescriptorHeap[texID + 32];
	//float4 texColor = tex.Sample(Sampler, input_.TexCoord);
	float4 texColor = Textures[texID].Sample(Sampler, input_.TexCoord);
	input_.Color.rgb = texColor.rgb * input_.Color.rgb;
	output.Color = float4(CalcHalfLambertianReflectance(input_).rgb * input_.Color.rgb, input_.Color.a);
	
	//output.Color = round(output.Color * 20.0f) * 0.05f;
	
	return output;
}