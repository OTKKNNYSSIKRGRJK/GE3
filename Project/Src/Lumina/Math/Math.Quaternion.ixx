module;

// Disables warning against nameless structs/unions.
#pragma warning(disable : 4201)

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.Math.Quaternion;

//****	******	******	******	******	****//

import <cmath>;

import Lumina.Math.Numerics;
import Lumina.Math.Vector;

//////	//////	//////	//////	//////	//////

#define INLINE_NAMESPACE_MATH_BEGIN		inline namespace Math {
#define INLINE_NAMESPACE_MATH_END		}

//****	******	******	******	******	****//

namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

		export class Quaternion : public Float4 {
		public:
			constexpr auto operator+(Quaternion const& other_)
				const noexcept -> Quaternion {
				return {
					x + other_.x,
					y + other_.y,
					z + other_.z,
					w + other_.w
				};
			}
			constexpr auto operator*(Quaternion const& other_)
				const noexcept -> Quaternion {
				return {
					(w * other_.x + x * other_.w) + (y * other_.z - z * other_.y),
					(w * other_.y + y * other_.w) + (z * other_.x - x * other_.z),
					(w * other_.z + z * other_.w) + (x * other_.y - y * other_.x),
					w * other_.w - (x * other_.x + y * other_.y + z * other_.z)
				};
			}
			constexpr auto operator*(float scalar_)
				const noexcept -> Quaternion {
				return {
					x * scalar_,
					y * scalar_,
					z * scalar_,
					w * scalar_
				};
			}

			//----	------	------	------	------	----//

		public:
			constexpr auto Re() const noexcept
				-> float { return w; }
			constexpr auto Im() const noexcept
				-> Float3 const& { return *static_cast<Float3 const*>(static_cast<void const*>(this)); }

			//----	------	------	------	------	----//

		public:
			inline auto Norm() const noexcept
				-> float { return std::sqrt(x * x + y * y + z * z + w * w); }
			inline auto Unit() const noexcept
				-> Quaternion { return (*this) * (1.0f / Norm()); }

			constexpr auto Conjugate() const noexcept
				-> Quaternion{ return { -x, -y, -z , w }; }
			constexpr auto Reciprocal() const noexcept
				-> Quaternion { return Conjugate() * (1.0f / (x * x + y * y + z * z + w * w)); }

			//----	------	------	------	------	----//

		public:
			static inline auto RotateAbout(Float3 const& axis_, float angle_) noexcept -> Quaternion {
				float const cosHalfAngle{ std::cos(angle_ * 0.5f) };
				float const sinHalfAngle{ std::sin(angle_ * 0.5f) };
				return {
					axis_.x * sinHalfAngle,
					axis_.y * sinHalfAngle,
					axis_.z * sinHalfAngle,
					cosHalfAngle
				};
			}
			static inline auto RotateAbout(float const axis_[3], float angle_) noexcept -> Quaternion {
				return RotateAbout(
					*reinterpret_cast<Float3 const*>(axis_),
					angle_
				);
			}

			static constexpr auto Rotate(Vec4 const& vec4_, Quaternion const& quat_) noexcept -> Vec4 {
				Quaternion&& vec4_Rotated{ quat_ * Quaternion{ vec4_ } *quat_.Reciprocal() };
				return Vec4{ reinterpret_cast<float const*>(&vec4_Rotated) };
			}

			//----	------	------	------	------	----//

		public:
			constexpr Quaternion() noexcept {}
			constexpr Quaternion(float x_, float y_, float z_, float w_) noexcept :
				Float4{ x_, y_, z_, w_ } {
			}
			constexpr Quaternion(Float3 const& im_, float re_) noexcept :
				Float4{ im_.x, im_.y, im_.z, re_ } {
			}
			Quaternion(Vec4 const& vec4_) {
				std::memcpy(this, &vec4_, sizeof(Quaternion));
			}
	};

	INLINE_NAMESPACE_MATH_END
}