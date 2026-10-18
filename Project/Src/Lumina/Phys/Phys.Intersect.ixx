//////	//////	//////	//////	//////	//////
//	Phys.Intersect							//
//////	//////	//////	//////	//////	//////
// 形状どうしの重なり判定（全組み合わせ）とレイキャスト。
// - Overlap(a, b, out) : 重なっていれば true。out が非 null なら接触情報を書く
//   * Normal は a → b の向き（a をこの向きの逆へ Depth だけ動かせば離れる）
//   * Point は「エフェクトを出す位置」程度の近似。物理の接触多様体ではない
// - Raycast(ray, shape, out) : 当たれば true。始点が形状の内側なら T = 0

export module Lumina.Phys.Intersect;

export import Lumina.Phys.Shape;

import Lumina.Math.Numerics;

export namespace Lumina::Phys {
	struct Contact {
		Float3 Point{ 0.0f, 0.0f, 0.0f };
		Float3 Normal{ 0.0f, 1.0f, 0.0f };		// a → b
		float Depth{ 0.0f };						// めり込み量（>= 0）
	};

	// a と b を入れ替えた接触情報
	constexpr Contact Flipped(Contact const& contact_) noexcept {
		return Contact{ contact_.Point, F3::Neg(contact_.Normal), contact_.Depth };
	}

	//----	重なり判定						----//

	bool Overlap(Sphere const& a_, Sphere const& b_, Contact* out_ = nullptr) noexcept;
	bool Overlap(Sphere const& a_, AABB const& b_, Contact* out_ = nullptr) noexcept;
	bool Overlap(Sphere const& a_, OBB const& b_, Contact* out_ = nullptr) noexcept;

	bool Overlap(AABB const& a_, Sphere const& b_, Contact* out_ = nullptr) noexcept;
	bool Overlap(AABB const& a_, AABB const& b_, Contact* out_ = nullptr) noexcept;
	bool Overlap(AABB const& a_, OBB const& b_, Contact* out_ = nullptr) noexcept;

	bool Overlap(OBB const& a_, Sphere const& b_, Contact* out_ = nullptr) noexcept;
	bool Overlap(OBB const& a_, AABB const& b_, Contact* out_ = nullptr) noexcept;
	bool Overlap(OBB const& a_, OBB const& b_, Contact* out_ = nullptr) noexcept;

	//----	レイキャスト					----//

	struct RayHit {
		float T{ 0.0f };							// Ray::At(T) が当たった位置
		Float3 Point{ 0.0f, 0.0f, 0.0f };
		Float3 Normal{ 0.0f, 1.0f, 0.0f };		// 当たった面の外向き法線（始点が内側なら -Direction）
	};

	bool Raycast(Ray const& ray_, Sphere const& sphere_, RayHit* out_ = nullptr) noexcept;
	bool Raycast(Ray const& ray_, AABB const& aabb_, RayHit* out_ = nullptr) noexcept;
	bool Raycast(Ray const& ray_, OBB const& obb_, RayHit* out_ = nullptr) noexcept;
}
