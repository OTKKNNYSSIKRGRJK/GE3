//////	//////	//////	//////	//////	//////
//	KeyConfig - キーボード・マウスの割り当て	//
//////	//////	//////	//////	//////	//////
// プレイヤーが変えられるキー割り当て（キーボード・マウス）。パッドの割り当ては Game.PlayerInput の既定のまま（変えられない）。
// 画面は Game.KeyConfigScreen（タイトルとポーズメニューの「設定」から開く）。2026-11-11。
//
// - 割り当ては 1 つの共有の表（Current）。最初に読んだときに Assets/Configs/KeyConfig.json を読み（無ければ既定）、
//   Apply で書き換えると保存して Version を 1 増やす。Game.PlayerInput の Controls は Version が変わったら InputMap を作り直す
//   （タイトルで変えても、ポーズ中に変えても、次のフレームから効く）。
// - 変えられるのは、今プレイヤーが使っている行動だけ（移動 4 方向・攻撃・ガード・回避・ジャンプ）。
//   スキル（Tool）・アイテム・ロックオン・ズームはまだ使っていないので出さない（既定のまま）。
// - 1 つのキーは 1 つの行動にだけ。すでに別の行動に使われているキーを割り当てたら、その行動と入れ替える（Assign）。
// - 使えないキー（IsReserved）: ESC（ポーズ・戻る）、F1 / F2（Debug のカーソル解放・エディタの表示）、Q（デモ用の自害）。
// - マウスは左・右ボタンだけ（GE3 の RawInput が中・X1・X2 を記録しないため）。

export module Game.KeyConfig;

//****	******	******	******	******	****//

// .cpp で使うヘッダユニットもここで import しておく（GE3 では、ビルド対象の .ixx が import したものだけが作られるため）
import <array>;
import <cstddef>;
import <cstdint>;
import <fstream>;
import <optional>;
import <string>;
import <string_view>;

import Lumina.Input;

//////	//////	//////	//////	//////	//////

export namespace Game::KeyConfig {
	// 並び = 設定画面の並び
	enum class Slot : std::uint8_t {
		MoveForward,
		MoveBack,
		MoveLeft,
		MoveRight,
		Attack,
		Guard,
		Dodge,
		Jump,
		Count
	};
	inline constexpr std::size_t NumSlots{ static_cast<std::size_t>(Slot::Count) };

	using Bindings = std::array<Lumina::Input::Source, NumSlots>;

	inline constexpr char const* ConfigPath{ "Assets/Configs/KeyConfig.json" };

	// 既定（セキロの PC 版の初期設定に寄せたもの。Game.PlayerInput の表と同じ）
	auto Defaults() noexcept -> Bindings;

	// 共有の表。最初の呼び出しで ConfigPath を読む（無い・壊れている・使えないキーがあるなら、そのスロットは既定）
	auto Current() -> Bindings const&;
	// 変えるたびに 1 増える（Current を最初に読んだ時点で 1）
	auto Version() -> std::uint32_t;
	// 表を置き換えて保存する。保存に失敗しても表は変わる（false を返す）
	auto Apply(Bindings const& b_) -> bool;

	// 画面の表示名（UTF-8）
	auto SlotLabel(Slot s_) noexcept -> std::string_view;
	// 保存のキー名
	auto SlotKey(Slot s_) noexcept -> std::string_view;
	// キー・マウスボタンの表示名（UTF-8）。知らないキーは「キー 0xNN」
	auto SourceName(Lumina::Input::Source src_) -> std::string;

	// 割り当てられない入力（予約済み・キーボード／マウス以外・範囲外）
	auto IsReserved(Lumina::Input::Source src_) noexcept -> bool;

	// b_ の slot_ に src_ を割り当てる。src_ を使っていた別のスロットがあれば、そこへ slot_ の元の入力を移す（入れ替え）。
	// 入れ替えたスロットを返す。IsReserved なら何もしない（nullopt）
	auto Assign(Bindings& b_, Slot slot_, Lumina::Input::Source src_) -> std::optional<Slot>;

	// 保存・読み込み（Current / Apply が使う。テスト用に公開）
	auto SaveTo(char const* path_, Bindings const& b_) -> bool;
	auto LoadFrom(char const* path_) -> Bindings;
}
