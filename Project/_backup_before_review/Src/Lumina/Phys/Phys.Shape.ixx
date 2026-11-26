//////	//////	//////	//////	//////	//////
//	Phys.Shape								//
//////	//////	//////	//////	//////	//////
// 当たり判定用の形状（Sphere / AABB / OBB / Ray）と、Float3 の小さな算術。
// - 形状は全て Float3 で持つ（Vec3 は align(16) なので構造体メンバに入れると C4324 になる）
// - 行列は Lumina::Mat4 の流儀（行ベクトル v * M、3 行目が平行移動）
// - OBB の Axis は正規直交、HalfExtent は各軸方向の半分の長さ

export module Lumina.Phys.Shape;

import <cstdint>;
import <cmath>;

import Lumina.Math.Numerics;
import Lumina.Math.Vector;
import Lumina.Math.Matrix;

//////	//////	//////	//////	//////	//////
//	Float3 算術								//
//////	//////	//////	//////	//////	//////

export namespace Lumina::Phys {
	inline constexpr float Infinity{ 3.402823466e+38F };
	inline constexpr float Epsilon{ 1.0e-6F };

	constexpr float MinF(float a_, float b_) noexcept { return (a_ < b_) ? a_ : b_; }
	constexpr float MaxF(float a_, float b_) noexcept { return (a_ < b_) ? b_ : a_; }
	constexpr float ClampF(float v_, float lo_, float hi_) noexcept { return MinF(MaxF(v_, lo_), hi_); }
	constexpr float AbsF(float v_) noexcept { return (v_ < 0.0f) ? -v_ : v_; }
	constexpr float SignF(float v_) noexcept { return (v_ < 0.0f) ? -1.0f : 1.0f; }		// 0 は +1 扱い

	namespace F3 {
		inline Float3 From(Vec3 const& v_) noexcept { return Float3{ v_.x, v_.y, v_.z }; }
		inline Float3 From(Vec4 const& v_) noexcept { return Float3{ v_.x, v_.y, v_.z }; }

		constexpr Float3 Add(Float3 const& a_, Float3 const& b_) noexcept { return Float3{ a_.x + b_.x, a_.y + b_.y, a_.z + b_.z }; }
		constexpr Float3 Sub(Float3 const& a_, Float3 const& b_) noexcept { return Float3{ a_.x - b_.x, a_.y - b_.y, a_.z - b_.z }; }
		constexpr Float3 Mul(Float3 const& a_, float s_) noexcept { return Float3{ a_.x * s_, a_.y * s_, a_.z * s_ }; }
		constexpr Float3 Neg(Float3 const& a_) noexcept { return Float3{ -a_.x, -a_.y, -a_.z }; }
		// a_ + b_ * s_
		constexpr Float3 MulAdd(Float3 const& a_, Float3 const& b_, float s_) noexcept {
			return Float3{ a_.x + b_.x * s_, a_.y + b_.y * s_, a_.z + b_.z * s_ };
		}
		constexpr Float3 Lerp(Float3 const& a_, Float3 const& b_, float t_) noexcept {
			return Float3{ a_.x + (b_.x - a_.x) * t_, a_.y + (b_.y - a_.y) * t_, a_.z + (b_.z - a_.z) * t_ };
		}

		constexpr float Dot(Float3 const& a_, Float3 const& b_) noexcept { return a_.x * b_.x + a_.y * b_.y + a_.z * b_.z; }
		constexpr Float3 Cross(Float3 const& a_, Float3 const& b_) noexcept {
			return Float3{
				a_.y * b_.z - a_.z * b_.y,
				a_.z * b_.x - a_.x * b_.z,
				a_.x * b_.y - a_.y * b_.x,
			};
		}
		constexpr float LengthSq(Float3 const& a_) noexcept { return Dot(a_, a_); }
		inline float Length(Float3 const& a_) noexcept { return std::sqrt(Dot(a_, a_)); }

		// 長さがほぼ 0 なら fallback_ を返す
		inline Float3 Normalize(Float3 const& a_, Float3 const& fallback_ = Float3{ 0.0f, 1.0f, 0.0f }) noexcept {
			float const lenSq{ Dot(a_, a_) };
			if (lenSq < Epsilon * Epsilon) { return fallback_; }
			return Mul(a_, 1.0f / std::sqrt(lenSq));
		}

		constexpr Float3 Min(Float3 const& a_, Float3 const& b_) noexcept { return Float3{ MinF(a_.x, b_.x), MinF(a_.y, b_.y), MinF(a_.z, b_.z) }; }
		constexpr Float3 Max(Float3 const& a_, Float3 const& b_) noexcept { return Float3{ MaxF(a_.x, b_.x), MaxF(a_.y, b_.y), MaxF(a_.z, b_.z) }; }
		constexpr Float3 Abs(Float3 const& a_) noexcept { return Float3{ AbsF(a_.x), AbsF(a_.y), AbsF(a_.z) }; }

		// 添字アクセス（0 = x, 1 = y, 2 = z）
		constexpr float Get(Float3 const& a_, int idx_) noexcept { return (idx_ == 0) ? a_.x : ((idx_ == 1) ? a_.y : a_.z); }
		constexpr void Set(Float3& a_, int idx_, float v_) noexcept {
			if (idx_ == 0) { a_.x = v_; }
			else if (idx_ == 1) { a_.y = v_; }
			else { a_.z = v_; }
		}
		constexpr Float3 Axis(int idx_) noexcept {
			return (idx_ == 0) ? Float3{ 1.0f, 0.0f, 0.0f } : ((idx_ == 1) ? Float3{ 0.0f, 1.0f, 0.0f } : Float3{ 0.0f, 0.0f, 1.0f });
		}

		// 行ベクトル規約: p * M（平行移動あり）
		inline Float3 TransformPoint(Float3 const& p_, Mat4 const& m_) noexcept {
			return Float3{
				p_.x * m_[0][0] + p_.y * m_[1][0] + p_.z * m_[2][0] + m_[3][0],
				p_.x * m_[0][1] + p_.y * m_[1][1] + p_.z * m_[2][1] + m_[3][1],
				p_.x * m_[0][2] + p_.y * m_[1][2] + p_.z * m_[2][2] + m_[3][2],
			};
		}
		// 行ベクトル規約: v * M（平行移動なし）
		inline Float3 TransformVector(Float3 const& v_, Mat4 const& m_) noexcept {
			return Float3{
				v_.x * m_[0][0] + v_.y * m_[1][0] + v_.z * m_[2][0],
				v_.x * m_[0][1] + v_.y * m_[1][1] + v_.z * m_[2][1],
				v_.x * m_[0][2] + v_.y * m_[1][2] + v_.z * m_[2][2],
			};
		}
		// M の i 行目（= ローカル i 軸の行き先）
		inline Float3 Row(Mat4 const& m_, int idx_) noexcept { return Float3{ m_[idx_][0], m_[idx_][1], m_[idx_][2] }; }
	}
}

//////	//////	//////	//////	//////	//////
//	形状									//
//////	//////	//////	//////	//////	//////

export namespace Lumina::Phys {
	struct Sphere {
		Float3 Center{ 0.0f, 0.0f, 0.0f };
		float Radius{ 0.5f };
	};

	struct AABB {
		Float3 Min{ -0.5f, -0.5f, -0.5f };
		Float3 Max{ 0.5f, 0.5f, 0.5f };

		constexpr Float3 Center() const noexcept { return F3::Mul(F3::Add(Min, Max), 0.5f); }
		constexpr Float3 HalfExtent() const noexcept { return F3::Mul(F3::Sub(Max, Min), 0.5f); }

		static constexpr AABB FromCenterHalf(Float3 const& center_, Float3 const& halfExtent_) noexcept {
			return AABB{ F3::Sub(center_, halfExtent_), F3::Add(center_, halfExtent_) };
		}
		// 空の AABB（Merge の初期値用）
		static constexpr AABB Empty() noexcept {
			return AABB{ Float3{ Infinity, Infinity, Infinity }, Float3{ -Infinity, -Infinity, -Infinity } };
		}
	};

	struct OBB {
		Float3 Center{ 0.0f, 0.0f, 0.0f };
		Float3 Axis[3]{
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
		};
		Float3 HalfExtent{ 0.5f, 0.5f, 0.5f };

		static constexpr OBB FromCenterHalf(Float3 const& center_, Float3 const& halfExtent_) noexcept {
			OBB ret{};
			ret.Center = center_;
			ret.HalfExtent = halfExtent_;
			return ret;
		}
		// オイラー角（Mat4::Rotate と同じ順序・符号）で回したもの
		static OBB FromCenterHalfEuler(Float3 const& center_, Float3 const& halfExtent_, Float3 const& euler_) noexcept {
			Mat4 const rot{ Mat4::Rotate(euler_) };
			OBB ret{};
			ret.Center = center_;
			ret.Axis[0] = F3::Row(rot, 0);
			ret.Axis[1] = F3::Row(rot, 1);
			ret.Axis[2] = F3::Row(rot, 2);
			ret.HalfExtent = halfExtent_;
			return ret;
		}
	};

	// Origin + Direction * t（0 <= t <= MaxT）。Direction は正規化しなくてよい
	// 線分 p0→p1 は Ray::Segment(p0, p1)（Direction = p1 - p0, MaxT = 1）
	struct Ray {
		Float3 Origin{ 0.0f, 0.0f, 0.0f };
		Float3 Direction{ 0.0f, 0.0f, 1.0f };
		float MaxT{ 1.0f };

		static constexpr Ray Segment(Float3 const& p0_, Float3 const& p1_) noexcept {
			return Ray{ p0_, F3::Sub(p1_, p0_), 1.0f };
		}
		constexpr Float3 At(float t_) const noexcept { return F3::MulAdd(Origin, Direction, t_); }
	};
}

//////	//////	//////	//////	//////	//////
//	変換・包含・最近接点					//
//////	//////	//////	//////	//////	//////

export namespace Lumina::Phys {
	//----	変換							----//

	constexpr OBB ToOBB(AABB const& aabb_) noexcept {
		return OBB::FromCenterHalf(aabb_.Center(), aabb_.HalfExtent());
	}

	constexpr AABB ToAABB(Sphere const& sphere_) noexcept {
		Float3 const r{ sphere_.Radius, sphere_.Radius, sphere_.Radius };
		return AABB{ F3::Sub(sphere_.Center, r), F3::Add(sphere_.Center, r) };
	}
	constexpr AABB ToAABB(AABB const& aabb_) noexcept { return aabb_; }
	// OBB を囲む AABB
	constexpr AABB ToAABB(OBB const& obb_) noexcept {
		Float3 const e0{ F3::Mul(F3::Abs(obb_.Axis[0]), obb_.HalfExtent.x) };
		Float3 const e1{ F3::Mul(F3::Abs(obb_.Axis[1]), obb_.HalfExtent.y) };
		Float3 const e2{ F3::Mul(F3::Abs(obb_.Axis[2]), obb_.HalfExtent.z) };
		Float3 const extent{ F3::Add(F3::Add(e0, e1), e2) };
		return AABB{ F3::Sub(obb_.Center, extent), F3::Add(obb_.Center, extent) };
	}

	constexpr AABB Merge(AABB const& a_, AABB const& b_) noexcept {
		return AABB{ F3::Min(a_.Min, b_.Min), F3::Max(a_.Max, b_.Max) };
	}

	//----	行列による変換					----//
	// 親行列にスケールが入っていてもよい。
	// - Sphere : 半径は 3 軸のうち最大の伸び率で拡大（非一様スケールでは外接球になる）
	// - AABB   : 回転すると軸平行を保てないので、変換後の箱を囲む AABB になる（Arvo の方法）
	// - OBB    : 軸ごとの伸びを HalfExtent に取り込み、軸は正規直交化する
	//            （親の連鎖でシアーが生じる場合のみ近似になる）

	inline Sphere Transform(Sphere const& sphere_, Mat4 const& m_) noexcept {
		float const s0{ F3::LengthSq(F3::Row(m_, 0)) };
		float const s1{ F3::LengthSq(F3::Row(m_, 1)) };
		float const s2{ F3::LengthSq(F3::Row(m_, 2)) };
		float const scale{ std::sqrt(MaxF(s0, MaxF(s1, s2))) };
		return Sphere{ F3::TransformPoint(sphere_.Center, m_), sphere_.Radius * scale };
	}

	inline AABB Transform(AABB const& aabb_, Mat4 const& m_) noexcept {
		Float3 const center{ F3::TransformPoint(aabb_.Center(), m_) };
		Float3 const h{ aabb_.HalfExtent() };
		Float3 extent{};
		for (int k{ 0 }; k < 3; ++k) {
			F3::Set(
				extent, k,
				AbsF(m_[0][k]) * h.x + AbsF(m_[1][k]) * h.y + AbsF(m_[2][k]) * h.z
			);
		}
		return AABB{ F3::Sub(center, extent), F3::Add(center, extent) };
	}

	inline OBB Transform(OBB const& obb_, Mat4 const& m_) noexcept {
		// 各軸 × 半径 を変換したベクトル
		Float3 const e0{ F3::TransformVector(F3::Mul(obb_.Axis[0], obb_.HalfExtent.x), m_) };
		Float3 const e1{ F3::TransformVector(F3::Mul(obb_.Axis[1], obb_.HalfExtent.y), m_) };
		Float3 const e2{ F3::TransformVector(F3::Mul(obb_.Axis[2], obb_.HalfExtent.z), m_) };

		// 長さ 0 の軸（HalfExtent = 0 の板など）でも軸が壊れないよう、変換した単位軸を予備に使う
		Float3 const u0{ F3::TransformVector(obb_.Axis[0], m_) };
		Float3 const u1{ F3::TransformVector(obb_.Axis[1], m_) };
		Float3 const u2{ F3::TransformVector(obb_.Axis[2], m_) };

		// Gram-Schmidt で正規直交化
		Float3 const a0{ F3::Normalize(u0, Float3{ 1.0f, 0.0f, 0.0f }) };
		Float3 const a1{ F3::Normalize(F3::MulAdd(u1, a0, -F3::Dot(u1, a0)), Float3{ 0.0f, 1.0f, 0.0f }) };
		Float3 a2{ F3::Cross(a0, a1) };
		if (F3::Dot(a2, u2) < 0.0f) { a2 = F3::Neg(a2); }	// 負スケール（鏡映）のときも元の向きを保つ

		OBB ret{};
		ret.Center = F3::TransformPoint(obb_.Center, m_);
		ret.Axis[0] = a0;
		ret.Axis[1] = a1;
		ret.Axis[2] = a2;
		ret.HalfExtent = Float3{ F3::Length(e0), F3::Length(e1), F3::Length(e2) };
		return ret;
	}

	//----	最近接点・包含					----//

	constexpr Float3 ClosestPoint(AABB const& aabb_, Float3 const& p_) noexcept {
		return F3::Min(F3::Max(p_, aabb_.Min), aabb_.Max);
	}
	constexpr Float3 ClosestPoint(OBB const& obb_, Float3 const& p_) noexcept {
		Float3 const d{ F3::Sub(p_, obb_.Center) };
		Float3 ret{ obb_.Center };
		ret = F3::MulAdd(ret, obb_.Axis[0], ClampF(F3::Dot(d, obb_.Axis[0]), -obb_.HalfExtent.x, obb_.HalfExtent.x));
		ret = F3::MulAdd(ret, obb_.Axis[1], ClampF(F3::Dot(d, obb_.Axis[1]), -obb_.HalfExtent.y, obb_.HalfExtent.y));
		ret = F3::MulAdd(ret, obb_.Axis[2], ClampF(F3::Dot(d, obb_.Axis[2]), -obb_.HalfExtent.z, obb_.HalfExtent.z));
		return ret;
	}
	inline Float3 ClosestPoint(Sphere const& sphere_, Float3 const& p_) noexcept {
		Float3 const d{ F3::Sub(p_, sphere_.Center) };
		float const distSq{ F3::LengthSq(d) };
		if (distSq <= sphere_.Radius * sphere_.Radius) { return p_; }
		return F3::MulAdd(sphere_.Center, d, sphere_.Radius / std::sqrt(distSq));
	}

	constexpr bool Contains(Sphere const& sphere_, Float3 const& p_) noexcept {
		return F3::LengthSq(F3::Sub(p_, sphere_.Center)) <= sphere_.Radius * sphere_.Radius;
	}
	constexpr bool Contains(AABB const& aabb_, Float3 const& p_) noexcept {
		return
			aabb_.Min.x <= p_.x && p_.x <= aabb_.Max.x &&
			aabb_.Min.y <= p_.y && p_.y <= aabb_.Max.y &&
			aabb_.Min.z <= p_.z && p_.z <= aabb_.Max.z;
	}
	constexpr bool Contains(OBB const& obb_, Float3 const& p_) noexcept {
		Float3 const d{ F3::Sub(p_, obb_.Center) };
		return
			AbsF(F3::Dot(d, obb_.Axis[0])) <= obb_.HalfExtent.x &&
			AbsF(F3::Dot(d, obb_.Axis[1])) <= obb_.HalfExtent.y &&
			AbsF(F3::Dot(d, obb_.Axis[2])) <= obb_.HalfExtent.z;
	}

	//----	頂点（デバッグ描画用）			----//
	// idx のビット 0/1/2 が x/y/z 軸の符号（0 = -, 1 = +）
	constexpr Float3 Corner(OBB const& obb_, uint32_t idx_) noexcept {
		Float3 ret{ obb_.Center };
		ret = F3::MulAdd(ret, obb_.Axis[0], (idx_ & 1U) ? obb_.HalfExtent.x : -obb_.HalfExtent.x);
		ret = F3::MulAdd(ret, obb_.Axis[1], (idx_ & 2U) ? obb_.HalfExtent.y : -obb_.HalfExtent.y);
		ret = F3::MulAdd(ret, obb_.Axis[2], (idx_ & 4U) ? obb_.HalfExtent.z : -obb_.HalfExtent.z);
		return ret;
	}
}
