//////	//////	//////	//////	//////	//////
//	TitleCrack（Title の Outro の割れ目）	//
//////	//////	//////	//////	//////	//////
// 方式 β：画面を斬撃線で 2 枚に割ってずらし、割れ目を白熱グラデーションで埋める。
// TitlePost.hlsl から include する。割れ目の描き方を差し替えるときは、このファイルを入れ替える
// （EvaluateCrack の入出力さえ同じなら、TitlePost.hlsl はそのまま使える）。
//
// 座標は「画面の高さ = 1」にそろえて扱う（角度・幅が縦横比で歪まないように）。

struct CrackParams {
	float2 Point;			// 斬撃線が通る点（画面 UV）
	float Angle;			// 斬撃線の向き（rad。画面の右向きから反時計回り）
	float Gap;				// 開き（画面の高さ = 1）
	float Rotate;			// 法線の + 側の破片だけ、Point まわりに回す角度（rad）
	float Aspect;			// 幅 / 高さ
	float3 CoreColor;		// 割れ目の中心の色（リニア）
	float3 EdgeColor;		// 割れ目のふちの色（リニア）
	float LightIntensity;	// 割れ目の光の明るさ
	float GlowWidth;		// ふちから破片側へにじむ光の幅
	float GlowIntensity;
	float LineWidth;		// 斬撃線の太さ
	float LineIntensity;
	float LineReveal;		// 斬撃線が端から端まで走る進み具合（0..1）
};

struct CrackSample {
	bool IsLight;			// true：割れ目の中（シーンは読まず Light を使う）
	float2 SourceUV;		// false のとき、シーンを読む位置
	float3 Light;			// 割れ目の光
	float3 Add;				// 上に足す光（斬撃線・ふちのにじみ）
};

static const float CrackHalfLength = 1.2f;	// 斬撃線の半分の長さ（画面の高さ = 1。画面の対角より長く）
static const float CrackLineSoftness = 0.05f;	// 斬撃線の先端のぼかし

CrackSample EvaluateCrack(float2 uv_, CrackParams p_) {
	const float2 aspect = float2(p_.Aspect, 1.0f);
	const float2 q = (uv_ - p_.Point) * aspect;

	// 画面の UV は y が下向きなので、反時計回りにするため y を反転
	const float2 tangent = float2(cos(p_.Angle), -sin(p_.Angle));
	const float2 normal = float2(-tangent.y, tangent.x);

	const float d = dot(q, normal);
	const float side = (d >= 0.0f) ? 1.0f : -1.0f;
	const float halfGap = max(p_.Gap, 0.0f) * 0.5f;

	// * 半分ずつ反対向きにずらす。+ 側の破片だけ Point まわりに少し回す
	float2 source = q - side * normal * halfGap;
	if (side > 0.0f) {
		const float s = sin(p_.Rotate);
		const float c = cos(p_.Rotate);
		source = float2(c * source.x + s * source.y, -s * source.x + c * source.y);
	}
	const float dSource = dot(source, normal);
	const float2 sourceUV = p_.Point + source / aspect;

	// * 斬撃線：命中から、線に沿って端から端まで走る
	const float along = dot(q, tangent);
	const float reach = lerp(-CrackHalfLength, CrackHalfLength, saturate(p_.LineReveal));
	const float lineMask = saturate((reach - along) / CrackLineSoftness);

	CrackSample result;
	result.Add = float3(0.0f, 0.0f, 0.0f);

	// * 元の領域の外（ずらした結果、線の反対側を指す）＝ 割れ目。画面外を指すときも、奥の光が見えている扱い
	const bool isInGap = (dSource * side < 0.0f);
	const bool isOutside = any(sourceUV < 0.0f) || any(sourceUV > 1.0f);
	if (isInGap || isOutside) {
		// 中心ほど白く、ふちほど EdgeColor
		const float x = isInGap ? saturate(abs(d) / max(halfGap, 1e-5f)) : 1.0f;
		result.IsLight = true;
		result.SourceUV = uv_;
		result.Light = lerp(p_.CoreColor, p_.EdgeColor, x * x) * p_.LightIntensity;
	}
	else {
		// 破片側：割れ目のふちから光がにじむ
		result.IsLight = false;
		result.SourceUV = sourceUV;
		result.Light = float3(0.0f, 0.0f, 0.0f);
		result.Add += p_.EdgeColor * p_.GlowIntensity * exp(-abs(dSource) / max(p_.GlowWidth, 1e-4f)) * lineMask;
	}

	// 斬撃線（開く前は細い線、開いた後は割れ目のふちの明るい筋）
	const float lineDistance = max(abs(d) - halfGap, 0.0f);
	result.Add += p_.CoreColor * p_.LineIntensity * exp(-lineDistance / max(p_.LineWidth, 1e-5f)) * lineMask;

	return result;
}
