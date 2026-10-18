// コライダのデバッグ表示用ライン
// 行優先（-Zpr）・行ベクトル（mul(v, M)）前提
// VSMain : SV_VertexID で StructuredBuffer から頂点を読む（頂点バッファなし）

cbuffer Constants : register(b0) {
	float4x4 ViewProj;
};

struct LineVertex {
	float3 Position;
	float Padding;
	float4 Color;
};

StructuredBuffer<LineVertex> Vertices : register(t0);

struct VSOutput {
	float4 Position : SV_Position;
	float4 Color : COLOR0;
};

VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	LineVertex v = Vertices[vertexID_];

	VSOutput output;
	output.Position = mul(float4(v.Position, 1.0f), ViewProj);
	output.Color = v.Color;
	return output;
}

float4 PSMain(VSOutput input_) : SV_TARGET0 {
	return input_.Color;
}
