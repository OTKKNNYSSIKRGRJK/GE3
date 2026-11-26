RWTexture2D<float> HeightMaps[] : register(u0, space0);

struct INDEX_SET {
	uint Next;
	uint Current;
	uint Previous;
};
ConstantBuffer<INDEX_SET> Indices : register(b0, space0);

struct DESC_INVOKE {
	float2 Position;
	float Strength;
};
ConstantBuffer<DESC_INVOKE> Desc_Invoke : register(b1, space0);

struct DESC_PROPAGATE {
	uint MapWidth;
	uint MapHeight;
	float Square_CFL;
};
ConstantBuffer<DESC_PROPAGATE> Desc_Propagate : register(b2, space0);

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

[numthreads(1U, 1U, 1U)]
void Clear(uint2 dtid_ : SV_DispatchThreadID) {
	HeightMaps[Indices.Next][dtid_] = 0.0f;
	HeightMaps[Indices.Current][dtid_] = 0.0f;
	HeightMaps[Indices.Previous][dtid_] = 0.0f;
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

[numthreads(1U, 1U, 1U)]
void Invoke() {
	int2 pos = int2(Desc_Invoke.Position);
	pos = clamp(
		pos,
		int2(5, 5),
		int2(Desc_Propagate.MapWidth - 5, Desc_Propagate.MapHeight - 5)
	);
	HeightMaps[Indices.Current][pos] += Desc_Invoke.Strength;
}

uint2 Displaced(uint2 pos_, int2 displacement_) {
	return uint2(
		clamp(
			int2(pos_) + displacement_,
			int2(0, 0),
			int2(Desc_Propagate.MapWidth, Desc_Propagate.MapHeight)
		)
	);
}

[numthreads(1U, 1U, 1U)]
void Propagate(uint2 dtid_ : SV_DispatchThreadID) {
	float u_Center = HeightMaps[Indices.Current][dtid_];
	float u_Left = HeightMaps[Indices.Current][Displaced(dtid_, int2(-1, 0))];
	float u_Right = HeightMaps[Indices.Current][Displaced(dtid_, int2(1, 0))];
	float u_Top = HeightMaps[Indices.Current][Displaced(dtid_, int2(0, -1))];
	float u_Bottom = HeightMaps[Indices.Current][Displaced(dtid_, int2(0, 1))];
	float u_Prev = HeightMaps[Indices.Previous][dtid_];
	
	HeightMaps[Indices.Next][dtid_] =
		(
			(dtid_.x == 0) ||
			(dtid_.x == Desc_Propagate.MapWidth) ||
			(dtid_.y == 0) ||
			(dtid_.y == Desc_Propagate.MapHeight)
		) ?
		(0.0f) :
		(
			2.0f * u_Center - u_Prev +
			Desc_Propagate.Square_CFL *
			(u_Left + u_Right + u_Top + u_Bottom - 4.0f * u_Center)
		);
	
	//HeightMaps[Indices.Next][dtid_] =
	//	2.0f * u_Center - u_Prev +
	//	Desc_Propagate.Square_CFL *
	//	(u_Left + u_Right + u_Top + u_Bottom - 4.0f * u_Center);
	
	HeightMaps[Indices.Next][dtid_] *= 0.95f;
}