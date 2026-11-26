export module Lumina.Phys.Collision;

import Lumina.Math.Vector;

namespace Lumina::Phys {
	export class Collision {
	public:
		static void Elastic(
			Lumina::Vec3& vel0_,
			Lumina::Vec3& vel1_,
			Lumina::Vec3 const& pos0_,
			Lumina::Vec3 const& pos1_,
			float mass0_,
			float mass1_
		) {
			Lumina::Vec3 const p10{ pos0_ - pos1_ };
			Lumina::Vec3 const v10{ vel0_ - vel1_ };
			float const prod_V10DotP10{ Lumina::Vec3::Dot(v10, p10) };
			float const inv_Len2_P10{ 1.0f / Lumina::Vec3::Dot(p10, p10) };
			float const inv_Sum_Mass{ 1.0f / (mass0_ + mass1_) };
			Lumina::Vec3 const tmp{ (prod_V10DotP10 * inv_Len2_P10 * inv_Sum_Mass) * p10 };
			vel0_ += (-2.0f * mass1_) * tmp;
			vel1_ += (2.0f * mass0_) * tmp;
		}
	};
}