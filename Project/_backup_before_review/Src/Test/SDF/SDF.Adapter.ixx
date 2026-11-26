//////	//////	//////	//////	//////	//////
//	SDF adapter								//
//////	//////	//////	//////	//////	//////
// Lumina.SDF（計算だけ）を GE3 で使うための読み込み・アップロード。エンジンに依存するのはここだけ。
// - LoadCoverage   : 画像（PNG など。DirectXTex の WIC 読み込み）から被覆率を取り出す。sRGB の変換はしない
// - BakeFromImage  : 読み込み → 符号付き距離 → 縮小 → R8 に詰める、までをまとめて行う
// - CreateTexture  : R8 / R8G8（UNORM）のテクスチャを作って中身を上げる（専用のコマンドリストで実行し、GPU の完了まで待つ）
//   半透明も使いたいときは、Baked::Coverage（出力の大きさにそろえた被覆率）と EncodeUNorm8x2 で R = 距離、G = 濃さにして R8G8 で上げる
//
// 例（起動時）:
//   std::string error{};
//   auto baked{ SDF::BakeFromImage("Assets/Img/Shape.png", {}, { .Width{ 2048U }, .Range{ 12.0f } }, error) };
//   if (baked) {
//       texture = SDF::CreateTextureR8(dxContext, baked->Distance.Width, baked->Distance.Height, baked->Encoded, "Shape SDF");
//       DX12::SRV<void>::Create(device, table.CPUHandle(0U), *texture);
//   }
//   // シェーダ: d（テクセル）= (0.5 − SampleLevel(...).r) · 2 · Range。画面の px にするなら × (表示の大きさ / テクスチャの大きさ)
//
// 例外は使わない（失敗は nullopt ＋ error に理由。ゲームは止めない）。ただし CreateTextureR8 の中の DX12 ラッパは失敗時に投げうる

export module Lumina.SDF.Adapter;

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <algorithm>;
import <cmath>;
import <cstdint>;
import <memory>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <vector>;

import Lumina.SDF;
import Lumina.DX12;
import Lumina.DX12.Context;

export namespace Lumina::SDF {
	// 画像のどのチャンネルを被覆率にするか
	enum class Channel : uint8_t {
		Alpha,			// 透明 PNG に形を塗ったもの（既定）
		Red,
		Luminance,		// 白黒の画像（白 = 内側）。sRGB のまま 0.2126 / 0.7152 / 0.0722 で混ぜる（しきい値に使うだけなので厳密でなくてよい）
	};

	struct ImageOptions {
		Channel Source{ Channel::Alpha };
		float Threshold{ 0.5f };		// これ以上を内側
		bool Invert{ false };			// 内外を入れ替える
	};

	struct BakeOptions {
		uint32_t Width{ 0U };			// 出力の幅（0 = 元の幅）
		uint32_t Height{ 0U };			// 出力の高さ（0 = 元の縦横比を保つ）
		float Range{ 8.0f };			// 詰める距離の範囲（出力のテクセル単位）
	};

	struct Baked {
		Grid<float> Distance{};			// 出力のテクセル単位の符号付き距離（内側が負）
		Grid<float> Coverage{};			// 元の被覆率を出力の大きさにそろえたもの（面積平均。しきい値で捨てる前の濃さ）
		std::vector<uint8_t> Encoded{};	// R8 に詰めたもの（Distance と同じ大きさ）
		float Range{ 8.0f };
		uint32_t SourceWidth{ 0U };
		uint32_t SourceHeight{ 0U };
	};

	// 失敗したら nullopt。error_ に理由（日本語）を入れる
	auto LoadCoverage(std::string_view path_, ImageOptions const& options_, std::string& error_) -> std::optional<Grid<float>>;

	auto BakeFromImage(
		std::string_view path_,
		ImageOptions const& image_,
		BakeOptions const& bake_,
		std::string& error_
	) -> std::optional<Baked>;

	enum class TexelFormat : uint8_t {
		R8,				// DXGI_FORMAT_R8_UNORM（1 テクセル 1 バイト）
		R8G8,			// DXGI_FORMAT_R8G8_UNORM（1 テクセル 2 バイト、RGRG…）
	};

	// texels_ は width_ × height_ × (1 テクセルのバイト数) バイト（行優先、上の行から）。終わった時点でテクスチャは PIXEL_SHADER_RESOURCE。
	// 大きさが合わない・0 のときは nullptr
	auto CreateTexture(
		DX12::Context const& dxContext_,
		TexelFormat format_,
		uint32_t width_,
		uint32_t height_,
		std::span<uint8_t const> texels_,
		std::string_view debugName_
	) -> std::unique_ptr<DX12::DefaultTexture2D>;

	// CreateTexture(R8) と同じ
	auto CreateTextureR8(
		DX12::Context const& dxContext_,
		uint32_t width_,
		uint32_t height_,
		std::span<uint8_t const> texels_,
		std::string_view debugName_
	) -> std::unique_ptr<DX12::DefaultTexture2D>;
}
