struct VSOutput {
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

static const float4 Positions[3] = {
	/* Top-left */		{ -1.0f, 1.0f, 0.0f, 1.0f },
	/* Top-right */		{ 3.0f, 1.0f, 0.0f, 1.0f },
	/* Bottom-left */	{ -1.0f, -3.0f, 0.0f, 1.0f },
};

static const float2 TexCoords[3] = {
	/* Top-left */		{ 0.0f, 0.0f },
	/* Top-right */		{ 2.0f, 0.0f },
	/* Bottom-left */	{ 0.0f, 2.0f },
};

VSOutput main(uint vertexID_ : SV_VertexID) {
	VSOutput output;
	output.Position = Positions[vertexID_];
	output.TexCoord = TexCoords[vertexID_];
	return output;
}