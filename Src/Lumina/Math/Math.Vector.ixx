export module Lumina.Math.Vector;

//****	******	******	******	******	****//

import <cmath>;

import <immintrin.h>;

//////	//////	//////	//////	//////	//////

#define INLINE_NAMESPACE_MATH_BEGIN		inline namespace Math {
#define INLINE_NAMESPACE_MATH_END		}

//****	******	******	******	******	****//

namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

	class Vec2;
	class Vec3;
	class Vec4;

	INLINE_NAMESPACE_MATH_END
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	Vec2									//
//////	//////	//////	//////	//////	//////

export namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

	class Vec2 {
	public:
		[[nodiscard]] constexpr float& operator[](uint32_t idx_) noexcept { return *((&x) + idx_); }
		[[nodiscard]] constexpr float operator[](uint32_t idx_) const noexcept { return *((&x) + idx_); }
		[[nodiscard]] constexpr float* operator()() noexcept { return &x; }
		[[nodiscard]] constexpr float const* operator()() const noexcept { return &x; }

		friend constexpr Vec2 operator+(Vec2 const& lhs_, Vec2 const& rhs_) noexcept;
		friend constexpr Vec2 operator-(Vec2 const& lhs_, Vec2 const& rhs_) noexcept;
		friend constexpr Vec2 operator*(float lhs_, Vec2 const& rhs_) noexcept;
		friend constexpr Vec2 operator*(Vec2 const& lhs_, float rhs_) noexcept;
		friend constexpr Vec2 operator/(Vec2 const& lhs_, float rhs_) noexcept;

		constexpr Vec2& operator+=(Vec2 const& rhs_) noexcept {
			x += rhs_.x;
			y += rhs_.y;
			return (*this);
		}
		constexpr Vec2& operator-=(Vec2 const& rhs_) noexcept {
			x -= rhs_.x;
			y -= rhs_.y;
			return (*this);
		}
		constexpr Vec2& operator*=(float rhs_) noexcept {
			x *= rhs_;
			y *= rhs_;
			return (*this);
		}
		constexpr Vec2& operator/=(float rhs_) noexcept {
			x /= rhs_;
			y /= rhs_;
			return *this;
		}

		//----	------	------	------	------	----//

	public:
		inline float Norm() const noexcept {
			return std::sqrt(x * x + y * y);
		}
		inline Vec2 Unit() const noexcept {
			float const inv_Norm{ 1.0f / Norm() };
			return Vec2{ x * inv_Norm, y * inv_Norm };
		}

		static constexpr float Dot(Vec2 const& vec0_, Vec2 const& vec1_) noexcept {
			return (vec0_.x * vec1_.x + vec0_.y * vec1_.y);
		}
		static constexpr float Cross(Vec2 const& vec0_, Vec2 const& vec1_) noexcept {
			return (vec0_.x * vec1_.y - vec0_.y * vec1_.x);
		}

		//----	------	------	------	------	----//

	public:
		constexpr Vec2() noexcept : x{ 0.0f }, y{ 0.0f } {}
		constexpr Vec2(float x_, float y_) noexcept : x{ x_ }, y{ y_ } {}
		constexpr Vec2(float const entries_[2]) noexcept : x{ entries_[0] }, y{ entries_[1] } {}

		//====	======	======	======	======	====//

	public:
		float x;
		float y;
	};

	constexpr Vec2 operator+(Vec2 const& lhs_, Vec2 const& rhs_) noexcept {
		return Vec2{ lhs_.x + rhs_.x, lhs_.y + rhs_.y };
	}
	constexpr Vec2 operator-(Vec2 const& lhs_, Vec2 const& rhs_) noexcept {
		return Vec2{ lhs_.x - rhs_.x, lhs_.y - rhs_.y };
	}
	constexpr Vec2 operator*(float lhs_, Vec2 const& rhs_) noexcept {
		return Vec2{ rhs_.x * lhs_, rhs_.y * lhs_ };
	}
	constexpr Vec2 operator*(Vec2 const& lhs_, float rhs_) noexcept {
		return Vec2{ lhs_.x * rhs_, lhs_.y * rhs_ };
	}
	constexpr Vec2 operator/(Vec2 const& lhs_, float rhs_) noexcept {
		float const inv_RHS{ 1.0f / rhs_ };
		return Vec2{ lhs_.x * inv_RHS, lhs_.y * inv_RHS };
	}

	INLINE_NAMESPACE_MATH_END
}

//////	//////	//////	//////	//////	//////
//	Vec3									//
//////	//////	//////	//////	//////	//////

export namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

	// Credits: https://github.com/pelletier/vector3/blob/master/vector3.h

	__declspec(align(16U))
	class Vec3 {
		friend Vec4;

		//====	======	======	======	======	====//

	public:
		[[nodiscard]] constexpr float& operator[](uint32_t idx_) noexcept { return *(&x + idx_); }
		[[nodiscard]] constexpr float operator[](uint32_t idx_) const noexcept { return *(&x + idx_); }
		[[nodiscard]] constexpr float* operator()() noexcept { return &x; }
		[[nodiscard]] constexpr float const* operator()() const noexcept { return &x; }

	public:
		friend inline Vec3 operator+(Vec3 const& lhs_, Vec3 const& rhs_) noexcept;
		friend inline Vec3 operator-(Vec3 const& lhs_, Vec3 const& rhs_) noexcept;
		friend inline Vec3 operator*(float lhs_, Vec3 const& rhs_) noexcept;
		friend inline Vec3 operator*(Vec3 const& lhs_, float rhs_) noexcept;
		friend inline Vec3 operator/(Vec3 const& lhs_, float rhs_) noexcept;

		inline Vec3& operator+=(Vec3 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			__m128 xmm_Result{ ::_mm_add_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec3& operator-=(Vec3 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			__m128 xmm_Result{ ::_mm_sub_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec3& operator*=(float rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
			__m128 xmm_Result{ ::_mm_mul_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec3& operator/=(float rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
			__m128 xmm_Result{ ::_mm_div_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}

		//----	------	------	------	------	----//

	public:
		inline float Norm() const noexcept {
			__m128 xmm{ ::_mm_load_ps(&x) };
			return ::_mm_cvtss_f32(::_mm_sqrt_ss(::_mm_dp_ps(xmm, xmm, 0x71)));
		}
		inline Vec3 Unit() const noexcept {
			__m128 xmm{ ::_mm_load_ps(&x) };
			__m128 xmm_Result{ ::_mm_div_ps(xmm, ::_mm_sqrt_ps(::_mm_dp_ps(xmm, xmm, 0x77))) };

			Vec3 ret{};
			::_mm_store_ps(&ret.x, xmm_Result);
			return ret;
		}

		static inline float Dot(Vec3 const& lhs_, Vec3 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			return ::_mm_cvtss_f32(::_mm_dp_ps(xmm_LHS, xmm_RHS, 0x71));
		}
		static inline Vec3 Cross(Vec3 const& lhs_, Vec3 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			__m128 xmm_Result{
				::_mm_sub_ps(
					::_mm_mul_ps(
						::_mm_shuffle_ps(xmm_LHS, xmm_LHS, _MM_SHUFFLE(3, 0, 2, 1)),
						::_mm_shuffle_ps(xmm_RHS, xmm_RHS, _MM_SHUFFLE(3, 1, 0, 2))
					),
					::_mm_mul_ps(
						::_mm_shuffle_ps(xmm_LHS, xmm_LHS, _MM_SHUFFLE(3, 1, 0, 2)),
						::_mm_shuffle_ps(xmm_RHS, xmm_RHS, _MM_SHUFFLE(3, 0, 2, 1))
					)
				)
			};

			Vec3 ret{};
			::_mm_store_ps(&ret.x, xmm_Result);
			return ret;
		}

		//----	------	------	------	------	----//

	public:
		constexpr Vec3() noexcept = default;
		constexpr Vec3(float x_, float y_, float z_) noexcept :
			x{ x_ },
			y{ y_ },
			z{ z_ } {}
		constexpr Vec3(float const entries_[3]) noexcept :
			x{ entries_[0] },
			y{ entries_[1] },
			z{ entries_[2] } {}
		constexpr Vec3(Vec2 const& vec2_) noexcept :
			x{ vec2_.x },
			y{ vec2_.y } {}

		//====	======	======	======	======	====//

	public:
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };
	protected:
		float w_{ 0.0f };
	};

	inline Vec3 operator+(Vec3 const& lhs_, Vec3 const& rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
		__m128 xmm_Result{ ::_mm_add_ps(xmm_LHS, xmm_RHS) };
		
		Vec3 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec3 operator-(Vec3 const& lhs_, Vec3 const& rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
		__m128 xmm_Result{ ::_mm_sub_ps(xmm_LHS, xmm_RHS) };

		Vec3 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec3 operator*(float lhs_, Vec3 const& rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps1(&lhs_) };
		__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
		__m128 xmm_Result{ ::_mm_mul_ps(xmm_LHS, xmm_RHS) };

		Vec3 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec3 operator*(Vec3 const& lhs_, float rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
		__m128 xmm_Result{ ::_mm_mul_ps(xmm_LHS, xmm_RHS) };

		Vec3 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec3 operator/(Vec3 const& lhs_, float rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
		__m128 xmm_Result{ ::_mm_div_ps(xmm_LHS, xmm_RHS) };

		Vec3 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}

	INLINE_NAMESPACE_MATH_END
}

//////	//////	//////	//////	//////	//////
//	Vec4									//
//////	//////	//////	//////	//////	//////

export namespace Lumina {
	INLINE_NAMESPACE_MATH_BEGIN

	__declspec(align(16U))
	class Vec4 {
		friend Vec3;

		//====	======	======	======	======	====//

	public:
		[[nodiscard]] constexpr float& operator[](uint32_t idx_) noexcept { return *(&x + idx_); }
		[[nodiscard]] constexpr float operator[](uint32_t idx_) const noexcept { return *(&x + idx_); }
		[[nodiscard]] constexpr float* operator()() noexcept { return &x; }
		[[nodiscard]] constexpr float const* operator()() const noexcept { return &x; }

		friend inline Vec4 operator+(Vec4 const& lhs_, Vec4 const& rhs_) noexcept;
		friend inline Vec4 operator-(Vec4 const& lhs_, Vec4 const& rhs_) noexcept;
		friend inline Vec4 operator*(float lhs_, Vec4 const& rhs_) noexcept;
		friend inline Vec4 operator*(Vec4 const& lhs_, float rhs_) noexcept;
		friend inline Vec4 operator/(Vec4 const& lhs_, float rhs_) noexcept;

		inline Vec4& operator+=(Vec4 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			__m128 xmm_Result{ ::_mm_add_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec4& operator+=(Vec3 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			__m128 xmm_Result{ ::_mm_add_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec4& operator-=(Vec4 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			__m128 xmm_Result{ ::_mm_sub_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec4& operator-=(Vec3 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			__m128 xmm_Result{ ::_mm_sub_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec4& operator*=(float rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
			__m128 xmm_Result{ ::_mm_mul_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}
		inline Vec4& operator/=(float rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&x) };
			__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
			__m128 xmm_Result{ ::_mm_div_ps(xmm_LHS, xmm_RHS) };
			::_mm_store_ps(&x, xmm_Result);
			return *this;
		}

		//----	------	------	------	------	----//

	public:
		inline float Norm() const noexcept {
			__m128 xmm{ ::_mm_load_ps(&x) };
			return ::_mm_cvtss_f32(::_mm_sqrt_ss(::_mm_dp_ps(xmm, xmm, 0xF1)));
		}
		inline Vec4 Unit() const noexcept {
			__m128 xmm{ ::_mm_load_ps(&x) };
			__m128 xmm_Result{ ::_mm_div_ps(xmm, ::_mm_sqrt_ps(::_mm_dp_ps(xmm, xmm, 0xFF))) };

			Vec4 ret{};
			::_mm_store_ps(&ret.x, xmm_Result);
			return ret;
		}

		static inline float Dot(Vec4 const& lhs_, Vec4 const& rhs_) noexcept {
			__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
			__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
			return ::_mm_cvtss_f32(::_mm_dp_ps(xmm_LHS, xmm_RHS, 0xF1));
		}

		//----	------	------	------	------	----//

	public:
		constexpr Vec4() noexcept = default;
		constexpr Vec4(float x_, float y_, float z_, float w_) noexcept :
			x{ x_ },
			y{ y_ },
			z{ z_ },
			w{ w_ } {}
		constexpr Vec4(float const entries_[4]) noexcept :
			x{ entries_[0] },
			y{ entries_[1] },
			z{ entries_[2] },
			w{ entries_[3] } {}
		constexpr Vec4(Vec3 const& vec3_) noexcept :
			x{ vec3_.x },
			y{ vec3_.y },
			z{ vec3_.z } {}
		inline Vec4(__m128 xmm_) noexcept { ::_mm_store_ps(&x, xmm_); }

		//====	======	======	======	======	====//

	public:
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };
		float w{ 1.0f };
	};

	inline Vec4 operator+(Vec4 const& lhs_, Vec4 const& rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
		__m128 xmm_Result{ ::_mm_add_ps(xmm_LHS, xmm_RHS) };

		Vec4 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec4 operator-(Vec4 const& lhs_, Vec4 const& rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
		__m128 xmm_Result{ ::_mm_sub_ps(xmm_LHS, xmm_RHS) };

		Vec4 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec4 operator*(float lhs_, Vec4 const& rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps1(&lhs_) };
		__m128 xmm_RHS{ ::_mm_load_ps(&rhs_.x) };
		__m128 xmm_Result{ ::_mm_mul_ps(xmm_LHS, xmm_RHS) };

		Vec4 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec4 operator*(Vec4 const& lhs_, float rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
		__m128 xmm_Result{ ::_mm_mul_ps(xmm_LHS, xmm_RHS) };

		Vec3 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}
	inline Vec4 operator/(Vec4 const& lhs_, float rhs_) noexcept {
		__m128 xmm_LHS{ ::_mm_load_ps(&lhs_.x) };
		__m128 xmm_RHS{ ::_mm_load_ps1(&rhs_) };
		__m128 xmm_Result{ ::_mm_div_ps(xmm_LHS, xmm_RHS) };

		Vec3 ret{};
		::_mm_store_ps(&ret.x, xmm_Result);
		return ret;
	}

	INLINE_NAMESPACE_MATH_END
}