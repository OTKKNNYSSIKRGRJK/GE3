module;

// Disables warning against uninitialized local variables only in the header.
#pragma warning(disable : 4700)
#include<External/niswegmann.small-matrix-inverse/invert4x4_sse.h>
#pragma warning(default : 4700)

// Disables warning against nameless structs/unions.
#pragma warning(disable : 4201)

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.Math.Matrix;

//****	******	******	******	******	****//

import Lumina.Math.Numerics;
import Lumina.Math.Vector;
import Lumina.Math.Quaternion;

import <cmath>;

import <memory>;

import <immintrin.h>;

//////	//////	//////	//////	//////	//////

#define INLINE_NAMESPACE_MATH_BEGIN		inline namespace Math {
#define INLINE_NAMESPACE_MATH_END		}

//****	******	******	******	******	****//

namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

	class Mat4;

	INLINE_NAMESPACE_MATH_END

	template<typename T>
	concept Float32 = (std::is_floating_point_v<T> && sizeof(T) == 4LLU);
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	Mat4									//
//////	//////	//////	//////	//////	//////

export namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

	__declspec(align(32U))
	class Mat4 {
	public:
		constexpr float* operator[](int idx_) noexcept { return Entries_[idx_]; }
		constexpr float const* operator[](int idx_) const noexcept { return Entries_[idx_]; }

		friend Vec4 operator*(Vec3 const& v_, Mat4 const& m_) noexcept;
		friend Vec4 operator*(Vec4 const& v_, Mat4 const& m_) noexcept;
		friend Mat4 operator*(Mat4 const& m0_, Mat4 const& m1_) noexcept;

		Mat4& operator+=(Mat4 const& rhs_) noexcept {
			Add(*this, *this, rhs_);
			return *this;
		}
		Mat4& operator-=(Mat4 const& rhs_) noexcept {
			Subtract(*this, *this, rhs_);
			return *this;
		}
		Mat4& operator*=(Mat4 const& rhs_) noexcept {
			Multiply(*this, *this, rhs_);
			return *this;
		}

		//----	------	------	------	------	----//

	public:
		float Det() const noexcept {
			constexpr auto swap{
				[] (__m128& xmm1_, __m128& xmm2_) constexpr {
					__m128 tmp{ xmm2_ };
					xmm2_ = xmm1_;
					xmm1_ = tmp;
				}
			};

			Mat4 m{ *this };
			float factor_RowSwapping{ 1.0f };

			// Row reduction
			for (int i_RowReduction{ 0 }; i_RowReduction < 3; ++i_RowReduction) {
				int i_Row{ i_RowReduction };

				// For the n-th iteration, looks for the first row where the n-th entry is nonzero.
				while (i_Row < 4 && m[i_Row][i_RowReduction] == 0) {
					factor_RowSwapping *= -1.0f;
					++i_Row;
				}
				// No nonzero entries are found, implying that the determinant is zero.
				if (i_Row > 3) { return 0.0f; }
				// Swaps rows if necessary in order to perform further elimination.
				else if (i_Row > i_RowReduction) { swap(m.XMMs_[i_Row], m.XMMs_[i_RowReduction]); }

				for (i_Row = i_RowReduction + 1; i_Row < 4; ++i_Row) {
					m.XMMs_[i_Row] = _mm_sub_ps(
						m.XMMs_[i_Row],
						_mm_mul_ps(
							m.XMMs_[i_RowReduction],
							_mm_set1_ps(m[i_Row][i_RowReduction] / m[i_RowReduction][i_RowReduction])
						)
					);
				}
			}

			return factor_RowSwapping * m[0][0] * m[1][1] * m[2][2] * m[3][3];
		}

		Mat4 Inv() const {
			Mat4 ret{};
			Invert_Impl(ret, *this);
			return ret;
		}

		//----	------	------	------	------	----//

	public:
		// dst_ = A_ + B_
		static void Add(Mat4& dst_, Mat4 const& A_, Mat4 const& B_) noexcept {
			dst_.YMMs_[0] = _mm256_add_ps(A_.YMMs_[0], B_.YMMs_[0]);
			dst_.YMMs_[1] = _mm256_add_ps(A_.YMMs_[1], B_.YMMs_[1]);
		}
		// dst_ = A_ - B_
		static void Subtract(Mat4& dst_, Mat4 const& A_, Mat4 const& B_) noexcept {
			dst_.YMMs_[0] = _mm256_sub_ps(A_.YMMs_[0], B_.YMMs_[0]);
			dst_.YMMs_[1] = _mm256_sub_ps(A_.YMMs_[1], B_.YMMs_[1]);
		}
		// Credits: https://stackoverflow.com/questions/18499971/efficient-4x4-matrix-multiplication-c-vs-assembly
		// dst_ = A_ * B_
		static void Multiply(Mat4& dst_, Mat4 const& A_, Mat4 const& B_) noexcept {
			__m256 a01{ _mm256_load_ps(A_.Entries_[0]) };
			__m256 a23{ _mm256_load_ps(A_.Entries_[2]) };
			__m128 b0{ _mm256_castps256_ps128(B_.YMMs_[0]) };
			__m128 b1{ _mm256_extractf128_ps(B_.YMMs_[0], 1) };
			__m128 b2{ _mm256_castps256_ps128(B_.YMMs_[1]) };
			__m128 b3{ _mm256_extractf128_ps(B_.YMMs_[1], 1) };

			__m256 dst01{ _mm256_mul_ps(_mm256_shuffle_ps(a01, a01, 0x00), _mm256_broadcast_ps(&b0)) };
			dst01 = _mm256_add_ps(dst01, _mm256_mul_ps(_mm256_shuffle_ps(a01, a01, 0x55), _mm256_broadcast_ps(&b1)));
			dst01 = _mm256_add_ps(dst01, _mm256_mul_ps(_mm256_shuffle_ps(a01, a01, 0xAA), _mm256_broadcast_ps(&b2)));
			dst01 = _mm256_add_ps(dst01, _mm256_mul_ps(_mm256_shuffle_ps(a01, a01, 0xFF), _mm256_broadcast_ps(&b3)));
			_mm256_store_ps(&dst_.Entries_[0][0], dst01);

			__m256 dst23{ _mm256_mul_ps(_mm256_shuffle_ps(a23, a23, 0x00), _mm256_broadcast_ps(&b0)) };
			dst23 = _mm256_add_ps(dst23, _mm256_mul_ps(_mm256_shuffle_ps(a23, a23, 0x55), _mm256_broadcast_ps(&b1)));
			dst23 = _mm256_add_ps(dst23, _mm256_mul_ps(_mm256_shuffle_ps(a23, a23, 0xAA), _mm256_broadcast_ps(&b2)));
			dst23 = _mm256_add_ps(dst23, _mm256_mul_ps(_mm256_shuffle_ps(a23, a23, 0xFF), _mm256_broadcast_ps(&b3)));
			_mm256_store_ps(&dst_.Entries_[2][0], dst23);
		}

		static void Transpose(Mat4& dst_, Mat4 const& src_) noexcept {
			// Implementation (almost) same as _MM_TRANSPOSE4_PS
			{
				__m128 tmps[4]{};

				tmps[0] = _mm_shuffle_ps(src_.XMMs_[0], src_.XMMs_[1], 0x44);
				tmps[2] = _mm_shuffle_ps(src_.XMMs_[0], src_.XMMs_[1], 0xEE);
				tmps[1] = _mm_shuffle_ps(src_.XMMs_[2], src_.XMMs_[3], 0x44);
				tmps[3] = _mm_shuffle_ps(src_.XMMs_[2], src_.XMMs_[3], 0xEE);
				dst_.XMMs_[0] = _mm_shuffle_ps(tmps[0], tmps[1], 0x88);
				dst_.XMMs_[1] = _mm_shuffle_ps(tmps[0], tmps[1], 0xDD);
				dst_.XMMs_[2] = _mm_shuffle_ps(tmps[2], tmps[3], 0x88);
				dst_.XMMs_[3] = _mm_shuffle_ps(tmps[2], tmps[3], 0xDD);
			}

			// Straightforward implementation
			/*for (int i{ 0 }; i < 4; ++i) {
				for (int j{ 0 }; j < 4; ++j) {
					dst_[i][j] = src_[j][i];
				}
			}*/
		}
		static void Invert(Mat4& dst_, Mat4 const& src_) {
			invert4x4(
				reinterpret_cast<float const*>(&src_),
				reinterpret_cast<float*>(&dst_)
			);
		}

		//----	------	------	------	------	----//

	private:
		// Straightforward implementation
		static void Invert_Impl(Mat4& dst_, Mat4 const& src_) {
			float const inv_Det{ 1.0f / src_.Det() };

			dst_[0][0] =
				inv_Det * (
					src_[1][1] * src_[2][2] * src_[3][3] +
					src_[1][2] * src_[2][3] * src_[3][1] +
					src_[1][3] * src_[2][1] * src_[3][2] -
					src_[1][3] * src_[2][2] * src_[3][1] -
					src_[1][2] * src_[2][1] * src_[3][3] -
					src_[1][1] * src_[2][3] * src_[3][2]
				);
			dst_[0][1] =
				inv_Det * (
					src_[0][3] * src_[2][2] * src_[3][1] +
					src_[0][2] * src_[2][1] * src_[3][3] +
					src_[0][1] * src_[2][3] * src_[3][2] -
					src_[0][1] * src_[2][2] * src_[3][3] -
					src_[0][2] * src_[2][3] * src_[3][1] -
					src_[0][3] * src_[2][1] * src_[3][2]
				);
			dst_[0][2] =
				inv_Det * (
					src_[0][1] * src_[1][2] * src_[3][3] +
					src_[0][2] * src_[1][3] * src_[3][1] +
					src_[0][3] * src_[1][1] * src_[3][2] -
					src_[0][3] * src_[1][2] * src_[3][1] -
					src_[0][2] * src_[1][1] * src_[3][3] -
					src_[0][1] * src_[1][3] * src_[3][2]
				);
			dst_[0][3] =
				inv_Det * (
					src_[0][3] * src_[1][2] * src_[2][1] +
					src_[0][2] * src_[1][1] * src_[2][3] +
					src_[0][1] * src_[1][3] * src_[2][2] -
					src_[0][1] * src_[1][2] * src_[2][3] -
					src_[0][2] * src_[1][3] * src_[2][1] -
					src_[0][3] * src_[1][1] * src_[2][2]
				);

			dst_[1][0] =
				inv_Det * (
					src_[1][3] * src_[2][2] * src_[3][0] +
					src_[1][2] * src_[2][0] * src_[3][3] +
					src_[1][0] * src_[2][3] * src_[3][2] -
					src_[1][0] * src_[2][2] * src_[3][3] -
					src_[1][2] * src_[2][3] * src_[3][0] -
					src_[1][3] * src_[2][0] * src_[3][2]
				);
			dst_[1][1] =
				inv_Det * (
					src_[0][0] * src_[2][2] * src_[3][3] +
					src_[0][2] * src_[2][3] * src_[3][0] +
					src_[0][3] * src_[2][0] * src_[3][2] -
					src_[0][3] * src_[2][2] * src_[3][0] -
					src_[0][2] * src_[2][0] * src_[3][3] -
					src_[0][0] * src_[2][3] * src_[3][2]
				);
			dst_[1][2] =
				inv_Det * (
					src_[0][3] * src_[1][2] * src_[3][0] +
					src_[0][2] * src_[1][0] * src_[3][3] +
					src_[0][0] * src_[1][3] * src_[3][2] -
					src_[0][0] * src_[1][2] * src_[3][3] -
					src_[0][2] * src_[1][3] * src_[3][0] -
					src_[0][3] * src_[1][0] * src_[3][2]
				);
			dst_[1][3] =
				inv_Det * (
					src_[0][0] * src_[1][2] * src_[2][3] +
					src_[0][2] * src_[1][3] * src_[2][0] +
					src_[0][3] * src_[1][0] * src_[2][2] -
					src_[0][3] * src_[1][2] * src_[2][0] -
					src_[0][2] * src_[1][0] * src_[2][3] -
					src_[0][0] * src_[1][3] * src_[2][2]
				);

			dst_[2][0] =
				inv_Det * (
					src_[1][0] * src_[2][1] * src_[3][3] +
					src_[1][1] * src_[2][3] * src_[3][0] +
					src_[1][3] * src_[2][0] * src_[3][1] -
					src_[1][3] * src_[2][1] * src_[3][0] -
					src_[1][1] * src_[2][0] * src_[3][3] -
					src_[1][0] * src_[2][3] * src_[3][1]
				);
			dst_[2][1] =
				inv_Det * (
					src_[0][3] * src_[2][1] * src_[3][0] +
					src_[0][1] * src_[2][0] * src_[3][3] +
					src_[0][0] * src_[2][3] * src_[3][1] -
					src_[0][0] * src_[2][1] * src_[3][3] -
					src_[0][1] * src_[2][3] * src_[3][0] -
					src_[0][3] * src_[2][0] * src_[3][1]
				);
			dst_[2][2] =
				inv_Det * (
					src_[0][0] * src_[1][1] * src_[3][3] +
					src_[0][1] * src_[1][3] * src_[3][0] +
					src_[0][3] * src_[1][0] * src_[3][1] -
					src_[0][3] * src_[1][1] * src_[3][0] -
					src_[0][1] * src_[1][0] * src_[3][3] -
					src_[0][0] * src_[1][3] * src_[3][1]
				);
			dst_[2][3] =
				inv_Det * (
					src_[0][3] * src_[1][1] * src_[2][0] +
					src_[0][1] * src_[1][0] * src_[2][3] +
					src_[0][0] * src_[1][3] * src_[2][1] -
					src_[0][0] * src_[1][1] * src_[2][3] -
					src_[0][1] * src_[1][3] * src_[2][0] -
					src_[0][3] * src_[1][0] * src_[2][1]
				);

			dst_[3][0] =
				inv_Det * (
					src_[1][2] * src_[2][1] * src_[3][0] +
					src_[1][1] * src_[2][0] * src_[3][2] +
					src_[1][0] * src_[2][2] * src_[3][1] -
					src_[1][0] * src_[2][1] * src_[3][2] -
					src_[1][1] * src_[2][2] * src_[3][0] -
					src_[1][2] * src_[2][0] * src_[3][1]
				);
			dst_[3][1] =
				inv_Det * (
					src_[0][0] * src_[2][1] * src_[3][2] +
					src_[0][1] * src_[2][2] * src_[3][0] +
					src_[0][2] * src_[2][0] * src_[3][1] -
					src_[0][2] * src_[2][1] * src_[3][0] -
					src_[0][1] * src_[2][0] * src_[3][2] -
					src_[0][0] * src_[2][2] * src_[3][1]
				);
			dst_[3][2] =
				inv_Det * (
					src_[0][2] * src_[1][1] * src_[3][0] +
					src_[0][1] * src_[1][0] * src_[3][2] +
					src_[0][0] * src_[1][2] * src_[3][1] -
					src_[0][0] * src_[1][1] * src_[3][2] -
					src_[0][1] * src_[1][2] * src_[3][0] -
					src_[0][2] * src_[1][0] * src_[3][1]
				);
			dst_[3][3] =
				inv_Det * (
					src_[0][0] * src_[1][1] * src_[2][2] +
					src_[0][1] * src_[1][2] * src_[2][0] +
					src_[0][2] * src_[1][0] * src_[2][1] -
					src_[0][2] * src_[1][1] * src_[2][0] -
					src_[0][1] * src_[1][0] * src_[2][2] -
					src_[0][0] * src_[1][2] * src_[2][1]
				);
		}

		//----	------	------	------	------	----//

	public:
		static Mat4 Rotate(Float3 const& eulerAngle_) {
			float const
				cosAlpha{ std::cosf(eulerAngle_.x)},
				sinAlpha{ std::sinf(eulerAngle_.x) },
				cosBeta{ std::cosf(eulerAngle_.y) },
				sinBeta{ std::sinf(eulerAngle_.y) },
				cosGamma{ std::cosf(eulerAngle_.z)},
				sinGamma{ std::sinf(eulerAngle_.z)};

			return Mat4{
				cosBeta * cosGamma,
				cosBeta * sinGamma,
				-sinBeta,
				0.0f,
				sinAlpha * sinBeta * cosGamma - cosAlpha * sinGamma,
				sinAlpha * sinBeta * sinGamma + cosAlpha * cosGamma,
				sinAlpha * cosBeta,
				0.0f,
				cosAlpha * sinBeta * cosGamma + sinAlpha * sinGamma,
				cosAlpha * sinBeta * sinGamma - sinAlpha * cosGamma,
				cosAlpha * cosBeta,
				0.0f,
				0.0f, 0.0f, 0.0f, 1.0f,
			};
		}

		/*static Mat4 Rotate() {

		}*/

		static Mat4 SRT(Vec3 const& scale_, Vec3 const& rotate_, Vec3 const& translate_) {
			float const
				cosAlpha{ std::cosf(rotate_.x) },
				sinAlpha{ std::sinf(rotate_.x) },
				cosBeta{ std::cosf(rotate_.y) },
				sinBeta{ std::sinf(rotate_.y) },
				cosGamma{ std::cosf(rotate_.z) },
				sinGamma{ std::sinf(rotate_.z) };

			Mat4 srt{
				cosBeta * cosGamma,
				cosBeta * sinGamma,
				-sinBeta,
				0.0f,
				sinAlpha * sinBeta * cosGamma - cosAlpha * sinGamma,
				sinAlpha * sinBeta * sinGamma + cosAlpha * cosGamma,
				sinAlpha * cosBeta,
				0.0f,
				cosAlpha * sinBeta * cosGamma + sinAlpha * sinGamma,
				cosAlpha * sinBeta * sinGamma - sinAlpha * cosGamma,
				cosAlpha * cosBeta,
				0.0f,
				translate_.x,
				translate_.y,
				translate_.z,
				1.0f,
			};

			srt.XMMs_[0] = _mm_mul_ps(srt.XMMs_[0], _mm_set1_ps(scale_.x));
			srt.XMMs_[1] = _mm_mul_ps(srt.XMMs_[1], _mm_set1_ps(scale_.y));
			srt.XMMs_[2] = _mm_mul_ps(srt.XMMs_[2], _mm_set1_ps(scale_.z));

			return srt;
		}

		static Mat4 Orthographic(
			float l_, float r_,
			float t_, float b_,
			float zn_, float zf_
		) {
			float const inv_W{ 1.0f / (r_ - l_) };
			float const inv_H{ 1.0f / (t_ - b_) };
			float const inv_D{ 1.0f / (zf_ - zn_) };

			return Mat4{
				2.0f * inv_W, 0.0f, 0.0f, 0.0f,
				0.0f, 2.0f * inv_H, 0.0f, 0.0f,
				0.0f, 0.0f, inv_D, 0.0f,
				-(l_ + r_) * inv_W, -(t_ + b_) * inv_H, -zn_ * inv_D, 1.0f,
			};
		}

		static Mat4 PerspectiveFOV(float fovY_, float aspectRatio_, float nearClip_, float farClip_) {
			float const cotTheta{ 1.0f / std::tanf(fovY_ * 0.5f) };
			float const inv_FrustumHeight{ 1.0f / (farClip_ - nearClip_) };
			return Mat4{
				(1.0f / aspectRatio_) * cotTheta, 0.0f, 0.0f, 0.0f,
				0.0f, cotTheta, 0.0f, 0.0f,
				0.0f, 0.0f, farClip_ * inv_FrustumHeight, 1.0f,
				0.0f, 0.0f, -nearClip_ * farClip_ * inv_FrustumHeight, 0.0f,
			};
		}

		static Mat4 Viewport(
			float left_, float top_,
			float width_, float height_,
			float minDepth_, float maxDepth_
		) {
			return Mat4{
				width_ * 0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, -height_ * 0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, maxDepth_ - minDepth_, 0.0f,
				left_ + width_ * 0.5f, top_ + height_ * 0.5f, minDepth_, 1.0f,
			};
		}

		//----	------	------	------	------	----//

	public:
		inline Mat4() noexcept {
			YMMs_[0] = _mm256_setr_ps(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
			YMMs_[1] = _mm256_setr_ps(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		template<Float32...Floats>
			requires(sizeof...(Floats) == 16U)
		constexpr Mat4(Floats...entries_) noexcept : Entries_{ entries_... } {}
		constexpr ~Mat4() noexcept = default;

		//====	======	======	======	======	====//

	protected:
		union {
			__m128 XMMs_[4];
			__m256 YMMs_[2];
			float Entries_[4][4];
		};
	};

	Vec4 operator*(Vec3 const& v_, Mat4 const& m_) noexcept {
		__m128 v{ *reinterpret_cast<__m128 const*>(&v_) };
		__m256 v01{ _mm256_set_m128(_mm_shuffle_ps(v, v, 0b01010101), _mm_shuffle_ps(v, v, 0b00000000))};
		__m256 v23{ _mm256_set_m128(_mm_shuffle_ps(v, v, 0b11111111), _mm_shuffle_ps(v, v, 0b10101010)) };
		__m256 ret256{ _mm256_add_ps(_mm256_mul_ps(v01, m_.YMMs_[0]), _mm256_mul_ps(v23, m_.YMMs_[1])) };
		return Vec4{ _mm_add_ps(_mm256_castps256_ps128(ret256), _mm256_extractf128_ps(ret256, 1)) };
	}

	Vec4 operator*(Vec4 const& v_, Mat4 const& m_) noexcept {
		__m128 v{ *reinterpret_cast<__m128 const*>(&v_) };
		__m256 v01{ _mm256_set_m128(_mm_shuffle_ps(v, v, 0b01010101), _mm_shuffle_ps(v, v, 0b00000000)) };
		__m256 v23{ _mm256_set_m128(_mm_shuffle_ps(v, v, 0b11111111), _mm_shuffle_ps(v, v, 0b10101010)) };
		__m256 ret256{ _mm256_add_ps(_mm256_mul_ps(v01, m_.YMMs_[0]), _mm256_mul_ps(v23, m_.YMMs_[1])) };
		return Vec4{ _mm_add_ps(_mm256_castps256_ps128(ret256), _mm256_extractf128_ps(ret256, 1)) };
	}

	Mat4 operator*(Mat4 const& m0_, Mat4 const& m1_) noexcept {
		Mat4 ret{};
		Mat4::Multiply(ret, m0_, m1_);
		return ret;
	}

	INLINE_NAMESPACE_MATH_END
}