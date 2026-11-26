//////	//////	//////	//////	//////	//////
//	PlayerInput - プレイヤーとカメラの操作	//
//////	//////	//////	//////	//////	//////
// ゲームの「行動」の定義と既定のバインド、毎フレームの取り込み。入力の抽象化（Lumina.Input）の上に乗るゲーム側の層。
//
// 方針
// - 行動の種類はセキロを基準にする。中身（回避の動き、弾きの判定など）はまだ。今は既存の動き（前後移動・旋回・ダッシュ・
//   ジャンプ・攻撃）をこの行動から読むように置き換えただけ。
// - キーボード・マウスとパッドの両方を、同じ行動にバインドする（どちらで操作しても同じ）。
// - 入力の取り込みは InGame の Update の頭で 1 回だけ行う（Controls::Update）。プレイヤーは State() から読む。
// - タイトル・ポーズメニュー・リザルトの「Space で再開」・Q の自害（デモ用）はまだ RawInput を直接読んでいる。
//
// 既定のバインド（セキロの初期設定に寄せた。PC は Fextralife の Sekiro Wiki「Controls」の一覧を参考にした）
//
//   行動      キーボード・マウス                パッド              セキロでの役割 → このゲームでの扱い
//   Dodge     左 Shift                         B                   回避（押しっぱなしでダッシュ）。今は前方へのダッシュ
//   Jump      Space                            A                   ジャンプ（空中でもう一度で二段）
//   Guard     右クリック                        LB                  押しっぱなしでガード、押した瞬間が弾き。今は未使用
//   Attack    左クリック                        RB                  攻撃
//   Tool      左 Ctrl                          RT                  義手忍具 → スキルの枠。今は未使用
//   UseItem   R                                十字キー上           アイテム（回復）。今は未使用
//   LockOn    中クリック・Tab                   右スティック押し込み  ロックオン・カメラのリセット。今は未使用
//   Move      WASD                             左スティック          移動（カメラ基準。進む方向を向く）
//   Look      マウス                           右スティック          視点。今は左右だけ使う（上下は未使用）
//   Zoom      ホイール                         なし                 カメラの寄り引き（セキロには無い）。今は未使用
//
// - 中クリックは GE3 の RawInput が記録しないので、ロックオンには Tab も付けてある（セキロの PC 版の初期設定は中クリック）。
// - Look の向き: X は右を向く向きが正、Y は上を向く向きが正（マウスは下へ動かすと正なので Scale を負にしてある）。
//   マウスは「そのフレームに動いた量 × 1 カウントあたりのラジアン」、スティックは「端まで倒したとき 1 フレームに回るラジアン」。
//   マウスの量は Raw Input のカウント（OS の加速なし）。感度は LookSensitivity を掛けて調整する（ImGui「Player Input」）。
//
// キーボード・マウスの移動・攻撃・ガード・回避・ジャンプは、プレイヤーが設定画面で変えられる（Game.KeyConfig。2026-11-11）。
// 上の表はその既定。Controls は Game.KeyConfig の Version が変わったら InputMap を作り直す（次のフレームから効く）。
//
// マウスで視点を回す間（Update の mouseLook_ が true）は、カーソルを隠してウィンドウに閉じ込める（CursorLock）。
// ポーズ中・ウィンドウが裏にあるとき・mouseLook_ が false のときは外し、マウスの移動量も視点に使わない。
// Debug では F1 でカーソルを解放・再固定する（ImGui を触るため）。固定中は ImGui にマウスを渡さない。

export module Game.PlayerInput;

//****	******	******	******	******	****//

import <cstddef>;
import <cstdint>;
import <optional>;
import <string_view>;

import Lumina.Input;
import Lumina.Input.Adapter;
import Lumina.WinApp.RawInput;

import Game.KeyConfig;

//////	//////	//////	//////	//////	//////

export namespace Game::PlayerInput {
	// 並び = 先行入力で同じフレームに押したときの優先順（小さいほど優先）
	enum class Act : Lumina::Input::ActionId {
		Dodge,
		Jump,
		Guard,
		Attack,
		Tool,
		UseItem,
		LockOn,
		Move,
		Look,
		Zoom,
		Count
	};

	auto Name(Act a_) noexcept -> std::string_view;
	// Name の逆（サイドカーの "allow": ["Attack", "Dodge"] などを読むため）。知らない名前なら nullopt
	auto ActFromName(std::string_view name_) noexcept -> std::optional<Act>;

	// 視点の感度（既定のバインドの Scale）
	inline constexpr float MouseLookRadPerCount{ 0.0015f };	// Raw Input の 1 カウント（800〜1600 DPI のマウスで、1 インチ動かすと約 70〜140°）
	inline constexpr float StickLookRadPerFrame{ 0.05f };		// 60fps で約 170°/秒

	// 先行入力に積む行動（ガードは押しっぱなしで使い、弾きは押した瞬間の時刻が大事なので積まない）
	inline constexpr Lumina::Input::ActionMask BufferedActions{
		Lumina::Input::MaskOf(Act::Dodge, Act::Jump, Act::Attack, Act::Tool, Act::UseItem)
	};
	inline constexpr std::uint32_t BufferLifetimeFrames{ 10U };

	// 上の表のバインドを作る（キーボード・マウスは Game.KeyConfig の既定）
	auto BuildDefaultMap() -> Lumina::Input::InputMap;
	// キーボード・マウスの移動・攻撃・ガード・回避・ジャンプを keys_ にしたバインドを作る（それ以外は上の表のまま）
	auto BuildMap(Game::KeyConfig::Bindings const& keys_) -> Lumina::Input::InputMap;

	// 取り込みと状態をまとめたもの。InGame が 1 つ持つ
	class Controls {
	public:
		void Initialize();

		// 毎フレーム 1 回、ゲームの Update の頭で呼ぶ。
		// syncOnly_ = true（ポーズ中など、ゲームを止めている間）は、エッジを出さずに今の入力へ合わせるだけ。
		// こうしておくと、止めている間に押したキーで、再開した瞬間に行動が出ない。
		// mouseLook_ = true（マウスで視点を回してよい間）はカーソルを固定する。false ならマウスの移動量は Look に入らない
		void Update(Lumina::WinApp::RawInput const& input_, bool syncOnly_, bool mouseLook_);

		// 次の Update を Sync にし、カーソルを解放する（InGame を離れるとき。戻ってきたとき、タイトルで押していたキーで行動が出ないように）
		void RequestSync() noexcept { NeedsSync_ = true; Cursor_.Release(); }

		// 視点の回す量（ラジアン、このフレームぶん）= Look × LookSensitivity
		auto LookDelta() const noexcept -> Lumina::Input::Axis2;
		auto CursorLocked() const noexcept -> bool { return Cursor_.Locked(); }

		auto State() const noexcept -> Lumina::Input::InputState const& { return State_; }
		auto Buffer() noexcept -> Lumina::Input::InputBuffer& { return Buffer_; }
		auto Map() const noexcept -> Lumina::Input::InputMap const& { return Map_; }
		auto GamepadConnected() const noexcept -> bool { return Gamepad_.Connected(); }

		// ImGui「Player Input」（Debug のみ。Release では何もしない）
		void DrawEditor();

	private:
		Lumina::Input::InputMap Map_{};
		Lumina::Input::InputState State_{};
		Lumina::Input::InputBuffer Buffer_{ BufferLifetimeFrames };
		Lumina::Input::RawFrame Frame_{};
		Lumina::Input::Adapter::RawInputSampler KeyboardMouse_{};
		Lumina::Input::Adapter::XInputSampler Gamepad_{};
		Lumina::Input::Adapter::CursorLock Cursor_{};
		float LookSensitivity_{ 1.0f };
		bool NeedsSync_{ true };	// 最初の 1 回も Sync（起動直後・タイトルから入った直後）
		bool DebugFreeCursor_{ false };	// Debug の F1 で切り替える
		bool PrevF1_{ false };
		std::uint32_t KeyConfigVersion_{ 0U };	// 作ったときの Game::KeyConfig::Version()
	};
}
