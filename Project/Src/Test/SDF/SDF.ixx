//////	//////	//////	//////	//////	//////
//	SDF (prototype)							//
//////	//////	//////	//////	//////	//////
// 白黒（被覆率）の画像から符号付き距離場（SDF）を作る。標準ライブラリだけに依存する（エンジンにも DirectXTex にも依存しない）。
// 画像の読み込みと GPU へのアップロードは Lumina.SDF.Adapter、単体テストは Lumina.SDF.Tests。
//
// 流れ: 被覆率 Grid<float>（0..1）→ SignedDistance（画素単位、内側が負）→ Downsample（必要なら）→ EncodeUNorm8（R8 に詰める）
//   SDF は内外の 2 値しか持たない（しきい値より薄い所は外側として捨てる）。半透明・濃淡も使いたいときは、
//   被覆率を Resample で同じ大きさにして、EncodeUNorm8x2 で R = 距離、G = 被覆率（濃さ）として一緒に詰める
//
// 距離変換: Felzenszwalb & Huttenlocher, "Distance Transforms of Sampled Functions"（2012, Theory of Computing）。
//   1 次元の二乗距離変換（放物線の下側の包絡線）を、列 → 行の順にかけると 2 次元の厳密なユークリッド距離（の二乗）になる。
//   画素数に比例する時間で済む。
// 符号付き距離: 内側の画素 = 被覆率 ≥ threshold。境界は「内側の画素の中心と外側の画素の中心の中間」に置く
//   （外側の画素: 最も近い内側の画素の中心までの距離 − 0.5、内側の画素: −(最も近い外側の画素の中心までの距離 − 0.5)）。
//   2 値化するので、境界の位置の誤差は最大で約 0.5 画素。高い解像度で作ってから縮小すれば、縮小した分だけ小さくなる
//   （例: 4000 px → 2048 px なら約 0.25 画素）。
// 詰め方: v = clamp(0.5 − d / (2·range), 0, 1)。内側が 0.5 より大きい。range（画素）より遠い距離は 0 / 1 に張り付く。
//   シェーダでは d = (0.5 − v)·2·range で戻す。

export module Lumina.SDF;

import <algorithm>;
import <cmath>;
import <cstdint>;
import <span>;
import <vector>;

export namespace Lumina::SDF {
	// 行優先の 2 次元配列（Data[y * Width + x]）。y = 0 が画像の上の行
	template<typename T>
	struct Grid {
		uint32_t Width{ 0U };
		uint32_t Height{ 0U };
		std::vector<T> Data{};

		Grid() = default;
		Grid(uint32_t width_, uint32_t height_, T const& value_ = T{})
			: Width{ width_ }, Height{ height_ }, Data(static_cast<std::size_t>(width_) * height_, value_) {}

		auto At(uint32_t x_, uint32_t y_) -> T& { return Data[static_cast<std::size_t>(y_) * Width + x_]; }
		auto At(uint32_t x_, uint32_t y_) const -> T const& { return Data[static_cast<std::size_t>(y_) * Width + x_]; }
		auto Empty() const -> bool { return Width == 0U || Height == 0U; }
	};

	// 「種が無い」ことを表す二乗距離（十分大きい値。double で足し引きしても溢れない）
	inline constexpr double Far{ 1.0e20 };

	// 1 次元の二乗距離変換: d[q] = min_p ((q − p)^2 + f[p])。f は種なら 0、それ以外は Far（途中の段では任意の値でもよい）。
	// v（f.size() 個）と z（f.size() + 1 個）は作業領域（呼び出し側で確保して使い回す）
	void SquaredDistance1D(
		std::span<double const> f_,
		std::span<double> d_,
		std::span<int> v_,
		std::span<double> z_
	);

	// 2 次元: seeds_ が 0 でない画素（の中心）までの二乗距離（画素単位）。種が 1 つも無ければ全部 Far 程度の値
	auto SquaredDistance(Grid<uint8_t> const& seeds_) -> Grid<double>;

	// 被覆率（0..1）から符号付き距離（画素単位、内側が負）。threshold_ 以上を内側とする。
	// 内側（または外側）の画素が 1 つも無いときは、もう一方の符号で、画像の幅 ＋ 高さに収めた値になる
	auto SignedDistance(Grid<float> const& coverage_, float threshold_ = 0.5f) -> Grid<float>;

	// 面積平均で大きさを変える（値はそのまま。被覆率などに使う）。拡大もできるが、その場合は双線形補間になるだけ
	auto Resample(Grid<float> const& source_, uint32_t width_, uint32_t height_) -> Grid<float>;

	// 面積平均で縮小（拡大もできるが、その場合は双線形補間になるだけ）。距離は縮小後の画素単位に直す。
	// 縦横の倍率が違うと距離が方向によって歪むので、そのときは縦横の倍率の平均で割る（縦横比は保つのが望ましい）
	auto Downsample(Grid<float> const& signedDistance_, uint32_t width_, uint32_t height_) -> Grid<float>;

	// R8 に詰める（上の「詰め方」）。range_ は画素単位（0 以下なら 1 として扱う）
	auto EncodeUNorm8(Grid<float> const& signedDistance_, float range_) -> std::vector<uint8_t>;

	// 2 チャンネル（R8G8）に詰める: R = 距離（EncodeUNorm8 と同じ）、G = second_（0..1 に収めて 0..255）。並びは RGRG…。
	// second_ は signedDistance_ と同じ大きさであること（違えば空を返す）
	auto EncodeUNorm8x2(Grid<float> const& signedDistance_, float range_, Grid<float> const& second_) -> std::vector<uint8_t>;

	// 戻す（シェーダと同じ式。確認・テスト用）
	auto DecodeUNorm8(uint8_t value_, float range_) -> float;
}
