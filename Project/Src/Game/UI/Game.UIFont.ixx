//////	//////	//////	//////	//////	//////
//	UIFont - 文字アトラスで文字列を並べる		//
//////	//////	//////	//////	//////	//////
// 事前に焼いた文字アトラス（Assets/UI/Font.KaiseiDecol.png ＋ .json、Kaisei Decol Bold）から、
// 文字列を 1 文字 1 枚の Lumina::Sprite（UV で文字を切り出す）に並べる。描画は呼び出し側の SpriteRenderer。2026-11-11。
//
// - アトラスは Assets/UI/Font.KaiseiDecol.make.py で作る（PIL）。入っている文字は ASCII ＋ ひらがな・カタカナ ＋ 画面で使う漢字・記号。
//   新しい文字を画面に出すときは、make.py の EXTRA に足して作り直す（入っていない文字は「□」の代わりに飛ばす）。
// - 座標は UI の正射影（1280x720、左上原点・Y 下向き）。y は行の上端（アセンダの線）。
// - JSON: Size（焼いた大きさ px）、Ascent / Descent、Width / Height（アトラスの px）、
//         Glyphs = [[コードポイント, x, y, w, h, xoff, yoff, advance], ...]（xoff / yoff は行の上端・ペン位置からのずれ）

export module Game.UIFont;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <cstddef>;
import <cstdint>;
import <fstream>;
import <string_view>;
import <unordered_map>;
import <vector>;

import Lumina;

//////	//////	//////	//////	//////	//////

export namespace Game::UIFont {
	struct Glyph {
		float X{ 0.0f };		// アトラスの中の位置と大きさ（px）
		float Y{ 0.0f };
		float W{ 0.0f };
		float H{ 0.0f };
		float XOff{ 0.0f };		// ペン位置（行の上端）からのずれ（焼いた大きさの px）
		float YOff{ 0.0f };
		float Advance{ 0.0f };
	};

	class Font {
	public:
		// JSON を読む。読めなければ false（Append は何もしない）
		auto Load(char const* jsonPath_) -> bool;
		auto Loaded() const noexcept -> bool { return Size_ > 0.0f; }

		// 大きさ size_（px）で並べたときの幅
		auto Measure(std::string_view utf8_, float size_) const -> float;
		auto LineHeight(float size_) const noexcept -> float;

		// out_ に 1 文字 1 枚の Sprite を足す。x_ は anchorX_（0 = 左端、0.5 = 中央、1 = 右端）の位置、y_ は行の上端。
		// textureId_ = 呼び出し側のテクスチャのテーブルでのアトラスの番号
		void Append(
			std::vector<Lumina::Sprite>& out_,
			std::string_view utf8_,
			float x_,
			float y_,
			float size_,
			Lumina::Float4 const& rgba_,
			std::uint32_t textureId_,
			float anchorX_ = 0.0f
		) const;

		// 入っていない文字の数（Append で飛ばした数の累計。確認用）
		auto MissingCount() const noexcept -> std::uint32_t { return Missing_; }

	private:
		std::unordered_map<std::uint32_t, Glyph> Glyphs_{};
		float Size_{ 0.0f };
		float Ascent_{ 0.0f };
		float Descent_{ 0.0f };
		float AtlasW_{ 1.0f };
		float AtlasH_{ 1.0f };
		mutable std::uint32_t Missing_{ 0U };
	};

	// UTF-8 を 1 文字ずつ取り出す（壊れたバイトは U+FFFD）。pos_ を進める。終わりなら false
	auto NextCodepoint(std::string_view s_, std::size_t& pos_, std::uint32_t& cp_) noexcept -> bool;
}
