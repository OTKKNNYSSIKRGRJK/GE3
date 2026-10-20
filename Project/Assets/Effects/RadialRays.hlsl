//////	//////	//////	//////	//////	//////
//	RadialRays（Lumina.RadialRays）			//
//////	//////	//////	//////	//////	//////
// 画面上の 1 点（Center）から放射状に伸びる光線＋中心の光。スクリーン空間・加算。
// Assets/Title/TitleRays.hlsl の写し（中身は同じ。Title 側には手を入れないため別ファイルにしている）。
// VSMain : SV_VertexID から画面全体を覆う三角形を 1 枚生成（頂点バッファなし）
// PSMain : 角度方向の値ノイズを鋭くして光線にし、中心からの距離で減衰させる

cbuffer Constants : register(b0) {
	float2 Center;			// 光線の中心（画面 UV、左上が 0,0）
	float Intensity;
	float Length;			// 光線の長さ（画面の高さに対する割合）
	float3 Color;			// 光線の色（リニア）
	float Time;				// 秒
	float RayCount;			// 一周あたりの光線の数（整数）
	float Sharpness;		// 大きいほど光線が細く・まばらになる
	float RotateSpeed;		// 模様の回転（rad/s）
	float FlickerSpeed;		// 光線の入れ替わりの速さ（/秒）
	float3 CoreColor;		// 中心の光の色（リニア）
	float CoreRadius;		// 中心の光の半径（画面の高さに対する割合）
	float CoreIntensity;
	float Aspect;			// 幅 / 高さ
	float2 Padding0;
};

static const float TwoPi = 6.28318530718f;

struct VSOutput {
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
};

VSOutput VSMain(uint vertexID_ : SV_VertexID) {
	// (0,0) (2,0) (0,2) の UV で、画面を覆う大きな三角形
	const float2 uv = float2((vertexID_ << 1U) & 2U, vertexID_ & 2U);

	VSOutput output;
	output.Position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	output.UV = uv;
	return output;
}

//////	//////	//////	//////	//////	//////
//	Helpers									//
//////	//////	//////	//////	//////	//////

float Hash11(float n_) {
	return frac(sin(n_ * 12.9898f) * 43758.5453f);
}

// 角度方向（a = 0..1 で一周）の値ノイズ。count 個の格子で一周するので継ぎ目が出ない
float AngularNoise(float a_, float count_, float seed_) {
	const float x = a_ * count_;
	const float i = floor(x);
	const float f = x - i;
	const float n0 = Hash11(fmod(i, count_) + seed_);
	const float n1 = Hash11(fmod(i + 1.0f, count_) + seed_);
	return lerp(n0, n1, f * f * (3.0f - 2.0f * f));
}

// 時間で 2 つの模様を入れ替えながら混ぜる（光線が明滅して見える）
float FlickeringNoise(float a_, float count_, float time_, float salt_) {
	const float step = floor(time_);
	const float blend = smoothstep(0.0f, 1.0f, time_ - step);
	const float n0 = AngularNoise(a_, count_, step * 7.13f + salt_);
	const float n1 = AngularNoise(a_, count_, (step + 1.0f) * 7.13f + salt_);
	return lerp(n0, n1, blend);
}

//////	//////	//////	//////	//////	//////
//	Pixel shader							//
//////	//////	//////	//////	//////	//////

float4 PSMain(VSOutput input_) : SV_Target {
	// 画面の高さを 1 とした座標で、中心からの距離と角度
	const float2 d = (input_.UV - Center) * float2(Aspect, 1.0f);
	const float r = length(d);
	const float a = frac(atan2(d.y, d.x) / TwoPi + 0.5f + RotateSpeed * Time / TwoPi);

	// 光線：太い模様＋細かい模様（別の速さで明滅）を重ねて鋭くする
	const float count = max(RayCount, 1.0f);
	const float flickerTime = Time * FlickerSpeed;
	const float coarse = FlickeringNoise(a, count, flickerTime, 0.0f);
	const float fine = FlickeringNoise(a, count * 3.0f, flickerTime * 1.37f, 19.1f);
	const float rays = pow(saturate(coarse * 0.7f + fine * 0.3f), max(Sharpness, 0.01f));

	// 長さ方向の減衰（中心の近くは細い光線が潰れてチラつくので、中心の光に任せて弱める）
	const float along = saturate(1.0f - r / max(Length, 1e-4f));
	const float nearCenter = saturate(r / max(CoreRadius, 1e-4f));
	const float rayMask = along * along * nearCenter;

	const float core = exp(-(r * r) / max(CoreRadius * CoreRadius, 1e-6f));

	const float3 color = (Color * rays * rayMask + CoreColor * core * CoreIntensity) * Intensity;
	return float4(color, 0.0f);
}
