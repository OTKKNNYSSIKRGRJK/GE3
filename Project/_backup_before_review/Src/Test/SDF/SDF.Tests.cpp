module Lumina.SDF.Tests;

// ここで import しているヘッダユニットは、いずれも SDF.Tests.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstdint>;
import <random>;
import <span>;
import <string>;
import <vector>;

namespace Lumina::SDF::Tests {
	namespace {
		struct Context {
			std::vector<Failure> Failures{};
			void Check(bool ok_, char const* test_, std::string detail_ = {}) {
				if (!ok_) { Failures.push_back({ test_, std::move(detail_) }); }
			}
		};

		auto Str(double v_) -> std::string { return std::to_string(v_); }

		// 1 次元: 総当たりと一致する（種の配置・途中の段の値がランダムでも）
		void Test1D(Context& ctx_) {
			std::mt19937 rng{ 1234U };
			std::uniform_int_distribution<int> length{ 1, 40 };
			std::uniform_real_distribution<double> value{ 0.0, 50.0 };
			std::bernoulli_distribution seed{ 0.2 };
			for (int trial{ 0 }; trial < 200; ++trial) {
				std::size_t const n{ static_cast<std::size_t>(length(rng)) };
				std::vector<double> f(n);
				for (auto& x : f) { x = seed(rng) ? 0.0 : ((trial % 2 == 0) ? Far : value(rng)); }
				std::vector<double> d(n);
				std::vector<int> v(n);
				std::vector<double> z(n + 1U);
				SquaredDistance1D(f, d, v, z);
				for (std::size_t q{ 0U }; q < n; ++q) {
					double best{ Far * 10.0 };
					for (std::size_t p{ 0U }; p < n; ++p) {
						double const dq{ static_cast<double>(q) - static_cast<double>(p) };
						best = std::min(best, dq * dq + f[p]);
					}
					// Far どうしの比較は桁落ちするので、Far 付近は「どちらも大きい」ことだけ見る
					bool const ok{ (best >= Far * 0.5) ? (d[q] >= Far * 0.5) : (std::abs(d[q] - best) < 1.0e-6) };
					if (!ok) {
						ctx_.Check(false, "SquaredDistance1D matches brute force", "n=" + std::to_string(n) + " q=" + std::to_string(q));
						return;
					}
				}
			}
		}

		// 2 次元: 総当たりと一致する
		void Test2D(Context& ctx_) {
			std::mt19937 rng{ 99U };
			std::bernoulli_distribution seed{ 0.05 };
			for (int trial{ 0 }; trial < 20; ++trial) {
				Grid<uint8_t> seeds{ 23U, 17U, 0U };
				for (auto& s : seeds.Data) { s = static_cast<uint8_t>(seed(rng) ? 1 : 0); }
				Grid<double> const d{ SquaredDistance(seeds) };
				bool any{ false };
				for (auto s : seeds.Data) { any = any || (s != 0U); }
				for (uint32_t y{ 0U }; y < seeds.Height; ++y) {
					for (uint32_t x{ 0U }; x < seeds.Width; ++x) {
						double best{ Far * 10.0 };
						for (uint32_t sy{ 0U }; sy < seeds.Height; ++sy) {
							for (uint32_t sx{ 0U }; sx < seeds.Width; ++sx) {
								if (seeds.At(sx, sy) == 0U) { continue; }
								double const dx{ static_cast<double>(x) - sx };
								double const dy{ static_cast<double>(y) - sy };
								best = std::min(best, dx * dx + dy * dy);
							}
						}
						bool const ok{ any ? (std::abs(d.At(x, y) - best) < 1.0e-6) : (d.At(x, y) >= Far * 0.5) };
						if (!ok) {
							ctx_.Check(false, "SquaredDistance matches brute force", "x=" + std::to_string(x) + " y=" + std::to_string(y));
							return;
						}
					}
				}
			}
		}

		// 円: 符号付き距離が「中心からの距離 − 半径」に近い（2 値化の誤差は 1 画素以内）
		void TestDisc(Context& ctx_) {
			uint32_t const size{ 64U };
			double const cx{ 31.5 };
			double const cy{ 30.0 };
			double const r{ 17.3 };
			Grid<float> coverage{ size, size, 0.0f };
			for (uint32_t y{ 0U }; y < size; ++y) {
				for (uint32_t x{ 0U }; x < size; ++x) {
					coverage.At(x, y) = (std::hypot(x - cx, y - cy) <= r) ? 1.0f : 0.0f;
				}
			}
			Grid<float> const sd{ SignedDistance(coverage) };
			double worst{ 0.0 };
			for (uint32_t y{ 0U }; y < size; ++y) {
				for (uint32_t x{ 0U }; x < size; ++x) {
					double const expected{ std::hypot(x - cx, y - cy) - r };
					worst = std::max(worst, std::abs(sd.At(x, y) - expected));
					if ((sd.At(x, y) < 0.0f) != (coverage.At(x, y) >= 0.5f)) {
						ctx_.Check(false, "Disc: sign follows coverage", "x=" + std::to_string(x) + " y=" + std::to_string(y));
						return;
					}
				}
			}
			ctx_.Check(worst <= 1.0, "Disc: distance error <= 1 px", "worst=" + Str(worst));
		}

		// 半平面（x < 20 が内側）: 境界は 19 と 20 の中間 = 19.5。どの行でも d = x − 19.5
		void TestHalfPlane(Context& ctx_) {
			Grid<float> coverage{ 40U, 8U, 0.0f };
			for (uint32_t y{ 0U }; y < 8U; ++y) {
				for (uint32_t x{ 0U }; x < 20U; ++x) { coverage.At(x, y) = 1.0f; }
			}
			Grid<float> const sd{ SignedDistance(coverage) };
			for (uint32_t y{ 0U }; y < 8U; ++y) {
				for (uint32_t x{ 0U }; x < 40U; ++x) {
					double const expected{ static_cast<double>(x) - 19.5 };
					if (std::abs(sd.At(x, y) - expected) > 1.0e-5) {
						ctx_.Check(false, "HalfPlane: exact", "x=" + std::to_string(x) + " d=" + Str(sd.At(x, y)));
						return;
					}
				}
			}

			// 4 分の 1 に縮小: 境界は 19.5 / 4 + 0.5·(1/4 − 1)… ではなく、画素中心の取り方を合わせて (x + 0.5)·4 − 0.5 − 19.5 を 4 で割った値
			Grid<float> const half{ Downsample(sd, 10U, 2U) };
			for (uint32_t x{ 0U }; x < 10U; ++x) {
				double const srcCenter{ (static_cast<double>(x) + 0.5) * 4.0 - 0.5 };
				double const expected{ (srcCenter - 19.5) / 4.0 };
				// 端（x = 0, 9）は双線形サンプルが端で止まるので少しずれる。内側だけ厳密に見る
				if (x >= 1U && x <= 8U && std::abs(half.At(x, 0U) - expected) > 1.0e-4) {
					ctx_.Check(false, "Downsample: distance scaled to new pixels", "x=" + std::to_string(x) + " d=" + Str(half.At(x, 0U)) + " expected=" + Str(expected));
					return;
				}
			}
		}

		// 内側が無い / 全部内側: 有限で、符号がそろう
		void TestDegenerate(Context& ctx_) {
			Grid<float> none{ 5U, 4U, 0.0f };
			Grid<float> const a{ SignedDistance(none) };
			bool okA{ true };
			for (float v : a.Data) { okA = okA && std::isfinite(v) && v > 0.0f && v <= 9.0f; }
			ctx_.Check(okA, "No inside: positive and finite");

			Grid<float> all{ 5U, 4U, 1.0f };
			Grid<float> const b{ SignedDistance(all) };
			bool okB{ true };
			for (float v : b.Data) { okB = okB && std::isfinite(v) && v < 0.0f && v >= -9.0f; }
			ctx_.Check(okB, "All inside: negative and finite");

			Grid<float> empty{};
			ctx_.Check(SignedDistance(empty).Empty(), "Empty input -> empty output");
		}

		// しきい値: 0.3 の画素はしきい値 0.25 なら内側、0.5 なら外側
		void TestThreshold(Context& ctx_) {
			Grid<float> c{ 3U, 1U, 0.0f };
			c.At(1U, 0U) = 0.3f;
			ctx_.Check(SignedDistance(c, 0.25f).At(1U, 0U) < 0.0f, "Threshold 0.25: 0.3 is inside");
			ctx_.Check(SignedDistance(c, 0.5f).At(1U, 0U) > 0.0f, "Threshold 0.5: 0.3 is outside");
		}

		// 詰め方: 境界 = 128 付近、内側ほど大きい、range を超えると張り付く、往復の誤差は量子化の範囲内
		void TestEncode(Context& ctx_) {
			float const range{ 8.0f };
			Grid<float> g{ 5U, 1U, 0.0f };
			g.Data = { -20.0f, -3.0f, 0.0f, 3.0f, 20.0f };
			std::vector<uint8_t> const e{ EncodeUNorm8(g, range) };
			ctx_.Check(e[0] == 255U && e[4] == 0U, "Encode: clamps beyond range");
			ctx_.Check(e[1] > e[2] && e[2] > e[3], "Encode: inside is larger");
			ctx_.Check(e[2] == 128U, "Encode: boundary is 128", std::to_string(e[2]));
			double worst{ 0.0 };
			for (std::size_t i{ 1U }; i <= 3U; ++i) { worst = std::max(worst, static_cast<double>(std::abs(DecodeUNorm8(e[i], range) - g.Data[i]))); }
			ctx_.Check(worst <= 2.0 * range / 255.0, "Decode: round trip within quantization", "worst=" + Str(worst));
		}

		// Resample: 値の単位は変えない（一定値は一定、平均は保たれる）。EncodeUNorm8x2: RGRG… に並ぶ
		void TestResampleAndPair(Context& ctx_) {
			Grid<float> flat{ 40U, 6U, 0.25f };
			Grid<float> const r{ Resample(flat, 10U, 3U) };
			bool okFlat{ r.Width == 10U && r.Height == 3U };
			for (float v : r.Data) { okFlat = okFlat && std::abs(v - 0.25f) < 1.0e-6f; }
			ctx_.Check(okFlat, "Resample: constant stays constant");

			Grid<float> stripes{ 8U, 2U, 0.0f };
			for (uint32_t y{ 0U }; y < 2U; ++y) {
				for (uint32_t x{ 0U }; x < 8U; x += 2U) { stripes.At(x, y) = 1.0f; }
			}
			Grid<float> const half{ Resample(stripes, 4U, 1U) };
			bool okMean{ true };
			for (float v : half.Data) { okMean = okMean && std::abs(v - 0.5f) < 1.0e-5f; }
			ctx_.Check(okMean, "Resample: 2:1 average of stripes is 0.5");

			Grid<float> sd{ 3U, 1U, 0.0f };
			sd.Data = { -8.0f, 0.0f, 8.0f };
			Grid<float> cov{ 3U, 1U, 0.0f };
			cov.Data = { 1.0f, 0.5f, -1.0f };
			std::vector<uint8_t> const rg{ EncodeUNorm8x2(sd, 8.0f, cov) };
			ctx_.Check(rg.size() == 6U, "EncodeUNorm8x2: 2 bytes per texel");
			if (rg.size() == 6U) {
				ctx_.Check(rg[0] == 255U && rg[2] == 128U && rg[4] == 0U, "EncodeUNorm8x2: R is distance");
				ctx_.Check(rg[1] == 255U && rg[3] == 128U && rg[5] == 0U, "EncodeUNorm8x2: G is clamped coverage");
			}
			Grid<float> wrong{ 2U, 1U, 0.0f };
			ctx_.Check(EncodeUNorm8x2(sd, 8.0f, wrong).empty(), "EncodeUNorm8x2: size mismatch -> empty");
		}
	}

	auto RunAll() -> std::vector<Failure> {
		Context ctx{};
		Test1D(ctx);
		Test2D(ctx);
		TestDisc(ctx);
		TestHalfPlane(ctx);
		TestDegenerate(ctx);
		TestThreshold(ctx);
		TestEncode(ctx);
		TestResampleAndPair(ctx);
		return ctx.Failures;
	}
}
