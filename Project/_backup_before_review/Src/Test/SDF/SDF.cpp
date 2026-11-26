module Lumina.SDF;

// ここで import しているヘッダユニットは、いずれもビルド対象の .ixx（SDF.ixx / SDF.Tests.ixx など）で import 済み
import <algorithm>;
import <cmath>;
import <cstdint>;
import <span>;
import <vector>;

namespace Lumina::SDF {
	void SquaredDistance1D(
		std::span<double const> f_,
		std::span<double> d_,
		std::span<int> v_,
		std::span<double> z_
	) {
		int const n{ static_cast<int>(f_.size()) };
		if (n <= 0) { return; }

		// 放物線 y = (x − p)^2 + f[p] の下側の包絡線を作る。v = 包絡線を作る放物線の頂点、z = 隣り合う放物線の境目
		auto const intersect{
			[&](int q_, int p_) -> double {
				double const q{ static_cast<double>(q_) };
				double const p{ static_cast<double>(p_) };
				return ((f_[static_cast<std::size_t>(q_)] + q * q) - (f_[static_cast<std::size_t>(p_)] + p * p)) / (2.0 * q - 2.0 * p);
			}
		};

		int k{ 0 };
		v_[0] = 0;
		z_[0] = -Far * 10.0;
		z_[1] = Far * 10.0;
		for (int q{ 1 }; q < n; ++q) {
			double s{ intersect(q, v_[static_cast<std::size_t>(k)]) };
			while (s <= z_[static_cast<std::size_t>(k)]) {		// z[0] は −∞ 扱いなので k は 0 未満にならない
				--k;
				s = intersect(q, v_[static_cast<std::size_t>(k)]);
			}
			++k;
			v_[static_cast<std::size_t>(k)] = q;
			z_[static_cast<std::size_t>(k)] = s;
			z_[static_cast<std::size_t>(k) + 1U] = Far * 10.0;
		}

		k = 0;
		for (int q{ 0 }; q < n; ++q) {
			while (z_[static_cast<std::size_t>(k) + 1U] < static_cast<double>(q)) { ++k; }
			int const p{ v_[static_cast<std::size_t>(k)] };
			double const dq{ static_cast<double>(q - p) };
			d_[static_cast<std::size_t>(q)] = dq * dq + f_[static_cast<std::size_t>(p)];
		}
	}

	auto SquaredDistance(Grid<uint8_t> const& seeds_) -> Grid<double> {
		uint32_t const w{ seeds_.Width };
		uint32_t const h{ seeds_.Height };
		Grid<double> result{ w, h, Far };
		if (seeds_.Empty()) { return result; }

		std::size_t const longest{ std::max(w, h) };
		std::vector<double> f(longest);
		std::vector<double> d(longest);
		std::vector<int> v(longest);
		std::vector<double> z(longest + 1U);

		// 列ごと（縦方向）
		for (uint32_t x{ 0U }; x < w; ++x) {
			for (uint32_t y{ 0U }; y < h; ++y) { f[y] = (seeds_.At(x, y) != 0U) ? 0.0 : Far; }
			SquaredDistance1D(
				std::span<double const>{ f.data(), h }, std::span<double>{ d.data(), h },
				std::span<int>{ v.data(), h }, std::span<double>{ z.data(), h + 1U }
			);
			for (uint32_t y{ 0U }; y < h; ++y) { result.At(x, y) = d[y]; }
		}
		// 行ごと（横方向）。列の結果を f にする
		for (uint32_t y{ 0U }; y < h; ++y) {
			for (uint32_t x{ 0U }; x < w; ++x) { f[x] = result.At(x, y); }
			SquaredDistance1D(
				std::span<double const>{ f.data(), w }, std::span<double>{ d.data(), w },
				std::span<int>{ v.data(), w }, std::span<double>{ z.data(), w + 1U }
			);
			for (uint32_t x{ 0U }; x < w; ++x) { result.At(x, y) = d[x]; }
		}
		return result;
	}

	auto SignedDistance(Grid<float> const& coverage_, float threshold_) -> Grid<float> {
		uint32_t const w{ coverage_.Width };
		uint32_t const h{ coverage_.Height };
		Grid<float> result{ w, h, 0.0f };
		if (coverage_.Empty()) { return result; }

		Grid<uint8_t> inside{ w, h, 0U };
		Grid<uint8_t> outside{ w, h, 0U };
		for (std::size_t i{ 0U }; i < coverage_.Data.size(); ++i) {
			bool const in{ coverage_.Data[i] >= threshold_ };
			inside.Data[i] = static_cast<uint8_t>(in ? 1 : 0);
			outside.Data[i] = static_cast<uint8_t>(in ? 0 : 1);
		}

		Grid<double> const toInside{ SquaredDistance(inside) };		// 外側の画素で意味を持つ
		Grid<double> const toOutside{ SquaredDistance(outside) };		// 内側の画素で意味を持つ

		// 種が無いと Far になるので、画像の幅 ＋ 高さに収める
		double const limit{ static_cast<double>(w) + static_cast<double>(h) };
		for (std::size_t i{ 0U }; i < result.Data.size(); ++i) {
			if (inside.Data[i] != 0U) {
				double const dist{ std::min(std::sqrt(toOutside.Data[i]), limit) };
				result.Data[i] = static_cast<float>(-(dist - 0.5));
			}
			else {
				double const dist{ std::min(std::sqrt(toInside.Data[i]), limit) };
				result.Data[i] = static_cast<float>(dist - 0.5);
			}
		}
		return result;
	}

	namespace {
		// 画素の中心が整数座標（0..w−1）。範囲外は端の値
		auto SampleBilinear(Grid<float> const& g_, double x_, double y_) -> double {
			double const maxX{ static_cast<double>(g_.Width - 1U) };
			double const maxY{ static_cast<double>(g_.Height - 1U) };
			double const x{ std::clamp(x_, 0.0, maxX) };
			double const y{ std::clamp(y_, 0.0, maxY) };
			uint32_t const x0{ static_cast<uint32_t>(x) };
			uint32_t const y0{ static_cast<uint32_t>(y) };
			uint32_t const x1{ std::min(x0 + 1U, g_.Width - 1U) };
			uint32_t const y1{ std::min(y0 + 1U, g_.Height - 1U) };
			double const tx{ x - static_cast<double>(x0) };
			double const ty{ y - static_cast<double>(y0) };
			double const a{ g_.At(x0, y0) + (g_.At(x1, y0) - g_.At(x0, y0)) * tx };
			double const b{ g_.At(x0, y1) + (g_.At(x1, y1) - g_.At(x0, y1)) * tx };
			return a + (b - a) * ty;
		}
	}

	namespace {
		// 面積平均。toNewPixels_ なら値を「縮小後の画素単位」に直す（距離用。縦横の倍率の平均で割る）
		auto ResampleScaled(Grid<float> const& source_, uint32_t width_, uint32_t height_, bool toNewPixels_) -> Grid<float> {
			Grid<float> result{ width_, height_, 0.0f };
			if (source_.Empty() || result.Empty()) { return result; }

			double const sx{ static_cast<double>(source_.Width) / width_ };		// 元の画素 / 新しい画素
			double const sy{ static_cast<double>(source_.Height) / height_ };
			double const scale{ toNewPixels_ ? (0.5 * (sx + sy)) : 1.0 };
			// 新しい 1 画素が覆う元の範囲を、n × m 点の双線形サンプルで平均する
			uint32_t const n{ static_cast<uint32_t>(std::max(1.0, std::ceil(sx))) };
			uint32_t const m{ static_cast<uint32_t>(std::max(1.0, std::ceil(sy))) };

			for (uint32_t j{ 0U }; j < height_; ++j) {
				for (uint32_t i{ 0U }; i < width_; ++i) {
					double sum{ 0.0 };
					for (uint32_t b{ 0U }; b < m; ++b) {
						double const y{ (static_cast<double>(j) + (static_cast<double>(b) + 0.5) / m) * sy - 0.5 };
						for (uint32_t a{ 0U }; a < n; ++a) {
							double const x{ (static_cast<double>(i) + (static_cast<double>(a) + 0.5) / n) * sx - 0.5 };
							sum += SampleBilinear(source_, x, y);
						}
					}
					result.At(i, j) = static_cast<float>(sum / (static_cast<double>(n) * m) / scale);
				}
			}
			return result;
		}
	}

	auto Resample(Grid<float> const& source_, uint32_t width_, uint32_t height_) -> Grid<float> {
		return ResampleScaled(source_, width_, height_, false);
	}

	auto Downsample(Grid<float> const& signedDistance_, uint32_t width_, uint32_t height_) -> Grid<float> {
		return ResampleScaled(signedDistance_, width_, height_, true);
	}

	auto EncodeUNorm8(Grid<float> const& signedDistance_, float range_) -> std::vector<uint8_t> {
		float const range{ (range_ > 0.0f) ? range_ : 1.0f };
		std::vector<uint8_t> result(signedDistance_.Data.size());
		for (std::size_t i{ 0U }; i < result.size(); ++i) {
			float const v{ std::clamp(0.5f - signedDistance_.Data[i] / (2.0f * range), 0.0f, 1.0f) };
			result[i] = static_cast<uint8_t>(std::lround(v * 255.0f));
		}
		return result;
	}

	auto EncodeUNorm8x2(Grid<float> const& signedDistance_, float range_, Grid<float> const& second_) -> std::vector<uint8_t> {
		if (signedDistance_.Width != second_.Width || signedDistance_.Height != second_.Height) { return {}; }
		std::vector<uint8_t> const r{ EncodeUNorm8(signedDistance_, range_) };
		std::vector<uint8_t> result(r.size() * 2U);
		for (std::size_t i{ 0U }; i < r.size(); ++i) {
			result[i * 2U + 0U] = r[i];
			result[i * 2U + 1U] = static_cast<uint8_t>(std::lround(std::clamp(second_.Data[i], 0.0f, 1.0f) * 255.0f));
		}
		return result;
	}

	auto DecodeUNorm8(uint8_t value_, float range_) -> float {
		float const range{ (range_ > 0.0f) ? range_ : 1.0f };
		return (0.5f - static_cast<float>(value_) / 255.0f) * 2.0f * range;
	}
}
