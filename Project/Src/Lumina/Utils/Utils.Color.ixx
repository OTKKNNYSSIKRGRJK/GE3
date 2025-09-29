export module Lumina.Utils.Color;

import <cstdint>;

import <cmath>;
import <algorithm>;

import Lumina.Math.Numerics;

namespace Lumina::Utils {
	export class Color {
	public:
		class RGB {
		public:
			constexpr RGB() noexcept = default;
			constexpr RGB(float r_, float g_, float b_) noexcept :
				R{ r_ }, G{ g_ }, B{ b_ } {}
			constexpr RGB(float const rgb_[3]) noexcept :
				R{ rgb_[0] }, G{ rgb_[1] }, B{ rgb_[2] } {}
			constexpr RGB(Float3 const& rgb_) noexcept :
				R{ rgb_.x }, G{ rgb_.y }, B{ rgb_.z } {}
			constexpr RGB(Float3&& rgb_) noexcept :
				R{ rgb_.x }, G{ rgb_.y }, B{ rgb_.z } {}

		public:
			float R{ 0.0f };		// [0.0f, 1.0f]
			float G{ 0.0f };		// [0.0f, 1.0f]
			float B{ 0.0f };		// [0.0f, 1.0f]
		};

		class HSV {
		public:
			float H{ 0.0f };		// [0.0f, 360.0f)
			float S{ 0.0f };		// [0.0f, 1.0f]
			float V{ 0.0f };		// [0.0f, 1.0f]
		};

	public:
		static RGB Convert(HSV hsv_) noexcept {
			static constexpr float inv_60{ 1.0f / 60.0f };
			static constexpr uint32_t permutations_CX0[6][3]{
				{ 0U, 1U, 2U, },
				{ 1U, 0U, 2U, },
				{ 2U, 0U, 1U, },
				{ 2U, 1U, 0U, },
				{ 1U, 2U, 0U, },
				{ 0U, 2U, 1U, },
			};
			
			while (hsv_.H >= 360.0f) { hsv_.H -= 360.0f; }
			while (hsv_.H < 0.0f) { hsv_.H += 360.0f; }

			float const chroma{ hsv_.S * hsv_.V };
			float const hue_Prime{ hsv_.H * inv_60 };

			float const m{ hsv_.V - chroma };

			int32_t hueSection{ static_cast<int32_t>(std::floor(hue_Prime)) };
			hueSection = (hueSection + 6) % 6;

			float const CX0[3]{
				chroma,
				chroma * (1.0f - std::abs(std::fmod(hue_Prime, 2.0f) - 1.0f)),
				0.0f,
			};
			
			return RGB{
				CX0[permutations_CX0[hueSection][0]] + m,
				CX0[permutations_CX0[hueSection][1]] + m,
				CX0[permutations_CX0[hueSection][2]] + m
			};
		}

		static HSV Convert(RGB rgb_) noexcept {
			float const x_Max{ std::max<float>(std::max<float>(rgb_.R, rgb_.G), rgb_.B) };
			float const x_Min{ std::min<float>(std::min<float>(rgb_.R, rgb_.G), rgb_.B) };
			
			float const chroma{ x_Max - x_Min };
			
			float hue{ 0.0f };
			if (x_Max != x_Min) {
				if (x_Max == rgb_.R) {
					hue = (rgb_.G - rgb_.B) / chroma + 6.0f;
				}
				else if (x_Max == rgb_.G) {
					hue = (rgb_.B - rgb_.R) / chroma + 8.0f;
				}
				else {
					hue = (rgb_.R - rgb_.G) / chroma + 10.0f;
				}
				hue = std::fmod(hue, 6.0f);
				hue *= 60.0f;
			}

			return HSV{
				hue,
				(x_Max > 0.0f) ? (chroma / x_Max) : (0.0f),
				x_Max
			};
		}
	};
}