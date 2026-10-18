module Lumina.Phys.Intersect;

import <cmath>;

#undef min
#undef max

namespace Lumina::Phys {
	namespace {
		// 外積軸がこれより短い（= 2 辺がほぼ平行）なら SAT の軸から外す。面の軸が代わりに効く
		constexpr float ParallelEpsilon{ 1.0e-4F };

		// 球の中心 c が OBB の内側にあるとき、いちばん近い面から外へ出す
		void InsideBoxContact(
			OBB const& box_,
			Float3 const& local_,			// 球の中心（箱のローカル座標）
			Float3 const& center_,			// 球の中心（ワールド）
			float radius_,
			Contact& out_
		) noexcept {
			int bestAxis{ 0 };
			float bestDist{ Infinity };
			for (int k{ 0 }; k < 3; ++k) {
				float const dist{ F3::Get(box_.HalfExtent, k) - AbsF(F3::Get(local_, k)) };
				if (dist < bestDist) {
					bestDist = dist;
					bestAxis = k;
				}
			}
			Float3 const faceNormal{ F3::Mul(box_.Axis[bestAxis], SignF(F3::Get(local_, bestAxis))) };
			// 球（a）→ 箱（b）の向きは「箱の外向き面法線」の逆
			out_.Normal = F3::Neg(faceNormal);
			out_.Depth = radius_ + bestDist;
			out_.Point = center_;
		}

		// 線分を [-h, h] のスラブで切る。entered なら tMin を更新した軸を返す
		bool ClipSlab(
			float origin_,
			float dir_,
			float half_,
			float& tMin_,
			float& tMax_,
			int axis_,
			int& hitAxis_,
			float& hitSign_
		) noexcept {
			if (AbsF(dir_) < Epsilon) {
				return (-half_ <= origin_ && origin_ <= half_);
			}
			float const inv{ 1.0f / dir_ };
			float tNear{ (-half_ - origin_) * inv };
			float tFar{ (half_ - origin_) * inv };
			float sign{ -1.0f };		// dir > 0 なら -面から入る
			if (tNear > tFar) {
				float const tmp{ tNear };
				tNear = tFar;
				tFar = tmp;
				sign = 1.0f;
			}
			if (tNear > tMin_) {
				tMin_ = tNear;
				hitAxis_ = axis_;
				hitSign_ = sign;
			}
			tMax_ = MinF(tMax_, tFar);
			return tMin_ <= tMax_;
		}
	}

	//////	//////	//////	//////	//////	//////
	//	Sphere vs *								//
	//////	//////	//////	//////	//////	//////

	bool Overlap(Sphere const& a_, Sphere const& b_, Contact* out_) noexcept {
		Float3 const d{ F3::Sub(b_.Center, a_.Center) };
		float const distSq{ F3::LengthSq(d) };
		float const sumR{ a_.Radius + b_.Radius };
		if (distSq > sumR * sumR) { return false; }

		if (out_) {
			float const dist{ std::sqrt(distSq) };
			out_->Normal = (dist > Epsilon) ? F3::Mul(d, 1.0f / dist) : Float3{ 0.0f, 1.0f, 0.0f };
			out_->Depth = sumR - dist;
			out_->Point = F3::MulAdd(a_.Center, out_->Normal, a_.Radius - out_->Depth * 0.5f);
		}
		return true;
	}

	bool Overlap(Sphere const& a_, OBB const& b_, Contact* out_) noexcept {
		// 球の中心を箱のローカルへ
		Float3 const d{ F3::Sub(a_.Center, b_.Center) };
		Float3 const local{ F3::Dot(d, b_.Axis[0]), F3::Dot(d, b_.Axis[1]), F3::Dot(d, b_.Axis[2]) };
		Float3 const clamped{
			ClampF(local.x, -b_.HalfExtent.x, b_.HalfExtent.x),
			ClampF(local.y, -b_.HalfExtent.y, b_.HalfExtent.y),
			ClampF(local.z, -b_.HalfExtent.z, b_.HalfExtent.z),
		};
		Float3 const diffLocal{ F3::Sub(clamped, local) };
		float const distSq{ F3::LengthSq(diffLocal) };
		if (distSq > a_.Radius * a_.Radius) { return false; }

		if (out_) {
			if (distSq < Epsilon * Epsilon) {
				// 中心が箱の内側
				InsideBoxContact(b_, local, a_.Center, a_.Radius, *out_);
			}
			else {
				float const dist{ std::sqrt(distSq) };
				Float3 closest{ b_.Center };
				closest = F3::MulAdd(closest, b_.Axis[0], clamped.x);
				closest = F3::MulAdd(closest, b_.Axis[1], clamped.y);
				closest = F3::MulAdd(closest, b_.Axis[2], clamped.z);
				out_->Normal = F3::Mul(F3::Sub(closest, a_.Center), 1.0f / dist);
				out_->Depth = a_.Radius - dist;
				out_->Point = closest;
			}
		}
		return true;
	}

	bool Overlap(Sphere const& a_, AABB const& b_, Contact* out_) noexcept {
		if (!out_) {
			// 判定だけなら軸平行のまま安く済ませる
			Float3 const closest{ ClosestPoint(b_, a_.Center) };
			return F3::LengthSq(F3::Sub(closest, a_.Center)) <= a_.Radius * a_.Radius;
		}
		return Overlap(a_, ToOBB(b_), out_);
	}

	//////	//////	//////	//////	//////	//////
	//	AABB vs AABB							//
	//////	//////	//////	//////	//////	//////

	bool Overlap(AABB const& a_, AABB const& b_, Contact* out_) noexcept {
		Float3 const lo{ F3::Max(a_.Min, b_.Min) };
		Float3 const hi{ F3::Min(a_.Max, b_.Max) };
		if (hi.x < lo.x || hi.y < lo.y || hi.z < lo.z) { return false; }

		if (out_) {
			// 押し出し量が最小の軸・向きを選ぶ。
			// 「重なり区間の幅」ではなく「どちらかへ抜けるのに必要な距離」で比べる
			// （片方がもう片方に包まれている軸では、区間の幅だけ動かしても抜けない）
			int bestAxis{ 0 };
			float bestDepth{ Infinity };
			float bestSign{ 1.0f };
			for (int k{ 0 }; k < 3; ++k) {
				float const toPositive{ F3::Get(a_.Max, k) - F3::Get(b_.Min, k) };	// b が + 側にいるとみなした押し出し量
				float const toNegative{ F3::Get(b_.Max, k) - F3::Get(a_.Min, k) };	// b が - 側にいるとみなした押し出し量
				float const depth{ MinF(toPositive, toNegative) };
				if (depth < bestDepth) {
					bestDepth = depth;
					bestAxis = k;
					bestSign = (toPositive <= toNegative) ? 1.0f : -1.0f;
				}
			}
			out_->Normal = F3::Mul(F3::Axis(bestAxis), bestSign);
			out_->Depth = bestDepth;
			out_->Point = F3::Mul(F3::Add(lo, hi), 0.5f);		// 重なり領域の中心
		}
		return true;
	}

	//////	//////	//////	//////	//////	//////
	//	OBB vs OBB（分離軸 15 本）				//
	//////	//////	//////	//////	//////	//////
	// Ericson『Real-Time Collision Detection』4.4.1 の定式化。
	// 接触情報が要るときは「重なりが最小の軸」を押し出し方向として残す。

	bool Overlap(OBB const& a_, OBB const& b_, Contact* out_) noexcept {
		float const a[3]{ a_.HalfExtent.x, a_.HalfExtent.y, a_.HalfExtent.z };
		float const b[3]{ b_.HalfExtent.x, b_.HalfExtent.y, b_.HalfExtent.z };

		// R[i][j] = A_i・B_j（B を A の座標系で表した回転）
		float R[3][3]{};
		float AbsR[3][3]{};
		for (int i{ 0 }; i < 3; ++i) {
			for (int j{ 0 }; j < 3; ++j) {
				R[i][j] = F3::Dot(a_.Axis[i], b_.Axis[j]);
				// 平行な辺の外積がほぼ 0 になって誤判定するのを防ぐ
				AbsR[i][j] = AbsF(R[i][j]) + Epsilon;
			}
		}

		// 中心間ベクトルを A の座標系で
		Float3 const tWorld{ F3::Sub(b_.Center, a_.Center) };
		float const t[3]{ F3::Dot(tWorld, a_.Axis[0]), F3::Dot(tWorld, a_.Axis[1]), F3::Dot(tWorld, a_.Axis[2]) };

		float bestDepth{ Infinity };
		Float3 bestNormal{ 0.0f, 1.0f, 0.0f };

		// dist_ : 中心間ベクトルを軸へ射影した値（符号付き, 軸は非正規化のまま）
		// invLen_ : 軸の長さの逆数（面の軸は 1）
		auto const testAxis{
			[&](float dist_, float ra_, float rb_, float invLen_, Float3 const& axisWorld_) -> bool {
				float const depth{ (ra_ + rb_ - AbsF(dist_)) * invLen_ };
				if (depth < 0.0f) { return false; }
				if (depth < bestDepth) {
					bestDepth = depth;
					bestNormal = F3::Mul(axisWorld_, invLen_ * SignF(dist_));
				}
				return true;
			}
		};

		// A の面の軸
		for (int i{ 0 }; i < 3; ++i) {
			float const rb{ b[0] * AbsR[i][0] + b[1] * AbsR[i][1] + b[2] * AbsR[i][2] };
			if (!testAxis(t[i], a[i], rb, 1.0f, a_.Axis[i])) { return false; }
		}

		// B の面の軸
		for (int j{ 0 }; j < 3; ++j) {
			float const ra{ a[0] * AbsR[0][j] + a[1] * AbsR[1][j] + a[2] * AbsR[2][j] };
			float const dist{ t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j] };
			if (!testAxis(dist, ra, b[j], 1.0f, b_.Axis[j])) { return false; }
		}

		// 辺 × 辺の軸 A_i × B_j
		for (int i{ 0 }; i < 3; ++i) {
			int const i1{ (i + 1) % 3 };
			int const i2{ (i + 2) % 3 };
			for (int j{ 0 }; j < 3; ++j) {
				int const j1{ (j + 1) % 3 };
				int const j2{ (j + 2) % 3 };

				// |A_i × B_j| = sin θ
				float const lenSq{ 1.0f - R[i][j] * R[i][j] };
				if (lenSq < ParallelEpsilon * ParallelEpsilon) { continue; }

				float const ra{ a[i1] * AbsR[i2][j] + a[i2] * AbsR[i1][j] };
				float const rb{ b[j1] * AbsR[i][j2] + b[j2] * AbsR[i][j1] };
				float const dist{ t[i2] * R[i1][j] - t[i1] * R[i2][j] };
				float const invLen{ 1.0f / std::sqrt(lenSq) };
				if (!testAxis(dist, ra, rb, invLen, F3::Cross(a_.Axis[i], b_.Axis[j]))) { return false; }
			}
		}

		if (out_) {
			out_->Normal = bestNormal;
			out_->Depth = bestDepth;
			// 互いの中心に最も近い相手側の点の中点
			Float3 const onB{ ClosestPoint(b_, a_.Center) };
			Float3 const onA{ ClosestPoint(a_, b_.Center) };
			out_->Point = F3::Mul(F3::Add(onA, onB), 0.5f);
		}
		return true;
	}

	bool Overlap(AABB const& a_, OBB const& b_, Contact* out_) noexcept {
		return Overlap(ToOBB(a_), b_, out_);
	}

	//////	//////	//////	//////	//////	//////
	//	逆向きの組み合わせ						//
	//////	//////	//////	//////	//////	//////

	bool Overlap(AABB const& a_, Sphere const& b_, Contact* out_) noexcept {
		bool const hit{ Overlap(b_, a_, out_) };
		if (hit && out_) { *out_ = Flipped(*out_); }
		return hit;
	}
	bool Overlap(OBB const& a_, Sphere const& b_, Contact* out_) noexcept {
		bool const hit{ Overlap(b_, a_, out_) };
		if (hit && out_) { *out_ = Flipped(*out_); }
		return hit;
	}
	bool Overlap(OBB const& a_, AABB const& b_, Contact* out_) noexcept {
		return Overlap(a_, ToOBB(b_), out_);
	}

	//////	//////	//////	//////	//////	//////
	//	Raycast									//
	//////	//////	//////	//////	//////	//////

	bool Raycast(Ray const& ray_, Sphere const& sphere_, RayHit* out_) noexcept {
		Float3 const m{ F3::Sub(ray_.Origin, sphere_.Center) };
		float const c{ F3::LengthSq(m) - sphere_.Radius * sphere_.Radius };

		// 始点が内側
		if (c <= 0.0f) {
			if (out_) {
				out_->T = 0.0f;
				out_->Point = ray_.Origin;
				out_->Normal = F3::Neg(F3::Normalize(ray_.Direction));
			}
			return true;
		}

		float const a{ F3::LengthSq(ray_.Direction) };
		if (a < Epsilon * Epsilon) { return false; }
		float const b{ F3::Dot(m, ray_.Direction) };
		if (b > 0.0f) { return false; }		// 外側にいて、遠ざかる向き

		float const disc{ b * b - a * c };
		if (disc < 0.0f) { return false; }

		float const tHit{ (-b - std::sqrt(disc)) / a };
		if (tHit > ray_.MaxT) { return false; }

		if (out_) {
			out_->T = tHit;
			out_->Point = ray_.At(tHit);
			out_->Normal = F3::Normalize(F3::Sub(out_->Point, sphere_.Center));
		}
		return true;
	}

	bool Raycast(Ray const& ray_, OBB const& obb_, RayHit* out_) noexcept {
		// 箱のローカルでスラブ法
		Float3 const d{ F3::Sub(ray_.Origin, obb_.Center) };
		float tMin{ 0.0f };
		float tMax{ ray_.MaxT };
		int hitAxis{ -1 };
		float hitSign{ 1.0f };
		for (int k{ 0 }; k < 3; ++k) {
			float const origin{ F3::Dot(d, obb_.Axis[k]) };
			float const dir{ F3::Dot(ray_.Direction, obb_.Axis[k]) };
			if (!ClipSlab(origin, dir, F3::Get(obb_.HalfExtent, k), tMin, tMax, k, hitAxis, hitSign)) {
				return false;
			}
		}

		if (out_) {
			out_->T = tMin;
			out_->Point = ray_.At(tMin);
			out_->Normal =
				(hitAxis < 0)
					? F3::Neg(F3::Normalize(ray_.Direction))		// 始点が内側
					: F3::Mul(obb_.Axis[hitAxis], hitSign);
		}
		return true;
	}

	bool Raycast(Ray const& ray_, AABB const& aabb_, RayHit* out_) noexcept {
		Float3 const c{ aabb_.Center() };
		Float3 const h{ aabb_.HalfExtent() };
		float tMin{ 0.0f };
		float tMax{ ray_.MaxT };
		int hitAxis{ -1 };
		float hitSign{ 1.0f };
		for (int k{ 0 }; k < 3; ++k) {
			float const origin{ F3::Get(ray_.Origin, k) - F3::Get(c, k) };
			if (!ClipSlab(origin, F3::Get(ray_.Direction, k), F3::Get(h, k), tMin, tMax, k, hitAxis, hitSign)) {
				return false;
			}
		}

		if (out_) {
			out_->T = tMin;
			out_->Point = ray_.At(tMin);
			out_->Normal =
				(hitAxis < 0)
					? F3::Neg(F3::Normalize(ray_.Direction))
					: F3::Mul(F3::Axis(hitAxis), hitSign);
		}
		return true;
	}
}
