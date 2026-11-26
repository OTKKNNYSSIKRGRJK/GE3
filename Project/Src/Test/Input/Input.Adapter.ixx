//////	//////	//////	//////	//////	//////
//	Input (prototype) - GE3 adapter			//
//////	//////	//////	//////	//////	//////
// コア（Lumina.Input）と GE3 の入力・Windows をつなぐ部分。エンジン・OS に依存するのはこのモジュールだけ。
// 新リポへ移植するときは、コアはそのまま持っていき、このモジュールを新リポの入力に合わせて書き直す（中身は RawFrame を埋めるだけ）。
//
// やること
// - RawInputSampler: GE3 の RawInput（キーボード・マウス）の今の状態を RawFrame に写す
// - XInputSampler:   XInput のパッドの今の状態を RawFrame に足す（GE3 の RawInput はパッドを扱っていないので、Windows の XInput を直接呼ぶ）
// - CursorLock:      マウスで視点を回す間、カーソルを隠してウィンドウの中に閉じ込める
// - キーコード = GE3 の KEY の値（Windows の仮想キーコード）。Key(KEY::SPACE) でコアの Source にする
//
// GE3 の RawInput から読めるもの・読めないもの（2026-10-30 にコードで確認）
// - キー: 256 個すべて。Shift / Ctrl / Alt は RawInput が左右に分けて記録するので、左右どちらかが押されていれば
//   KEY::SHIFT / CTRL / ALT（左右を区別しないコード）も押されている扱いにする。
// - マウスボタン: 左・右だけ（中・X1・X2 は RawInput が記録していない）。
// - マウスの移動量: RawMouse::TotalDeltaX / TotalDeltaY（Raw Input の相対移動量の累計。2026-11-04 に RawInput へ追加）の
//   前のフレームとの差。OS のポインタの加速が掛からず、カーソルを閉じ込めても止まらない。
//   （RawMouse::DeltaX / DeltaY は「最後の WM_INPUT 1 件ぶん」で、マウスを止めても 0 に戻らないため使わない）
//   マウスは RIDEV_INPUTSINK で登録されているので、ウィンドウが裏にあっても動きが届く。視点に使うかどうかは呼び出し側で決める
//   （Game.PlayerInput は CursorLock が掛かっている間だけ使う）。
// - ホイール: RawMouse::Wheel（累積のノッチ数）の前のフレームとの差。
// - 1 フレームのあいだに押して離したキーは拾えない（RawInput が今の状態しか持っていないため。コアの制約と同じ）。
//
// XInput について（Microsoft の「Getting Started With XInput」に従った）
// - 使えるのは XInput 対応のパッド（Xbox のコントローラーなど）。PlayStation のコントローラーは、そのままでは XInput に見えないことが多い。
// - 4 つのスロットのうち、最初につながっているもの 1 つだけを使う（1 人用なので）。
// - つながっていないスロットを毎フレーム問い合わせると重いので、見失ったら RetryFrames ごとに 0〜3 を探し直す。
// - スティックは [-1, 1]、トリガーは [0, 1] に正規化して入れるだけ。デッドゾーンはコア（GamepadTuning）が掛ける。
// - 振動は扱っていない。
// - パッドを持っていない状態で書いたので、実機では未確認。
//
// 呼び出し側のサンプル（既存コードは変更しないので、組み込むときの参考）
//
//   import Lumina.Input;
//   import Lumina.Input.Adapter;
//   namespace In = Lumina::Input;
//   using Lumina::WinApp::KEY;
//   using In::GamepadButton;
//   using In::Analog;
//
//   // 行動（ゲーム側で定義する。並び = 同時押しのときに先行入力から取り出す優先順）
//   enum class Act : In::ActionId { Move, Look, Dodge, Jump, Attack, Guard, LockOn, Tool, Count };
//
//   // 初期化（1 回）。配置はセキロの PC 版の初期設定に寄せた例
//   In::InputMap map{};
//   map.Define(Act::Move, In::ActionKind::Axis2D, true);            // 斜めでも長さ 1
//   map.Bind(Act::Move, In::Adapter::Key(KEY::W), In::Component::Y, +1.0f);
//   map.Bind(Act::Move, In::Adapter::Key(KEY::S), In::Component::Y, -1.0f);
//   map.Bind(Act::Move, In::Adapter::Key(KEY::A), In::Component::X, -1.0f);
//   map.Bind(Act::Move, In::Adapter::Key(KEY::D), In::Component::X, +1.0f);
//   map.Bind(Act::Move, In::FromAnalog(Analog::LeftStickX), In::Component::X);
//   map.Bind(Act::Move, In::FromAnalog(Analog::LeftStickY), In::Component::Y);
//   map.Define(Act::Look, In::ActionKind::Axis2D);                  // 収めない。Scale が感度
//   map.Bind(Act::Look, In::FromAnalog(Analog::MouseX), In::Component::X, 0.005f);       // マウス: 1px あたり
//   map.Bind(Act::Look, In::FromAnalog(Analog::MouseY), In::Component::Y, 0.005f);
//   map.Bind(Act::Look, In::FromAnalog(Analog::RightStickX), In::Component::X, 0.05f);   // スティック: 端まで倒して 1 フレームあたり
//   map.Bind(Act::Look, In::FromAnalog(Analog::RightStickY), In::Component::Y, -0.05f);  // スティックは上が正なのでマウスと符号を逆に
//
//   //               行動            キーボード・マウス                                    パッド（セキロの配置）
//   map.Define(Act::Dodge,  In::ActionKind::Button); map.Bind(Act::Dodge,  In::Adapter::Key(KEY::SHIFT_LEFT));        map.Bind(Act::Dodge,  In::FromPad(GamepadButton::B));
//   map.Define(Act::Jump,   In::ActionKind::Button); map.Bind(Act::Jump,   In::Adapter::Key(KEY::SPACE));             map.Bind(Act::Jump,   In::FromPad(GamepadButton::A));
//   map.Define(Act::Attack, In::ActionKind::Button); map.Bind(Act::Attack, In::FromMouse(In::MouseButton::Left));     map.Bind(Act::Attack, In::FromPad(GamepadButton::RightShoulder));
//   map.Define(Act::Guard,  In::ActionKind::Button); map.Bind(Act::Guard,  In::FromMouse(In::MouseButton::Right));    map.Bind(Act::Guard,  In::FromPad(GamepadButton::LeftShoulder));
//   map.Define(Act::LockOn, In::ActionKind::Button); map.Bind(Act::LockOn, In::Adapter::Key(KEY::E));                 map.Bind(Act::LockOn, In::FromPad(GamepadButton::RightThumb));
//   map.Define(Act::Tool,   In::ActionKind::Button); map.Bind(Act::Tool,   In::Adapter::Key(KEY::CTRL_LEFT));         map.Bind(Act::Tool,   In::FromAnalog(Analog::RightTrigger));
//   // LockOn はセキロでは中クリックだが、GE3 の RawInput は中ボタンを記録しないので当面 E。トリガーは半分以上で押した扱い
//
//   In::Adapter::RawInputSampler keyboardMouse{};
//   In::Adapter::XInputSampler gamepad{};
//   In::InputState state{};
//   In::InputBuffer buffer{ 10U };
//
//   // シーンに入ったとき（前のシーンで押していたキーで行動が出ないように）
//   In::RawFrame frame{};
//   keyboardMouse.Capture(input_, frame);
//   gamepad.Capture(frame);
//   state.Sync(map, frame);
//   buffer.Clear();
//
//   // 毎フレーム（Update の頭で 1 回）
//   bool blockKeys{ false }, blockMouse{ false };
//   #if defined(_DEBUG)
//   blockKeys = ImGui::GetIO().WantCaptureKeyboard;   // ImGui を操作している間は、ゲームには何も押されていないように見せる
//   blockMouse = ImGui::GetIO().WantCaptureMouse;
//   #endif
//   keyboardMouse.Capture(input_, frame, blockKeys, blockMouse);   // frame を作り直す
//   gamepad.Capture(frame);                                         // パッドを足す（必ず後に呼ぶ）
//   state.Update(map, frame);
//   buffer.Advance(isHitStop ? 0U : 1U);
//   buffer.Capture(state, In::MaskOf(Act::Dodge, Act::Jump, Act::Attack));   // Guard は押しっぱなしで使うので積まない
//
//   In::Axis2 const move{ state.Axis(Act::Move) };                  // カメラ基準の移動方向に直すのはゲーム側
//   bool const guarding{ state.Down(Act::Guard) };
//   bool const deflect{ state.Pressed(Act::Guard) };                 // 弾きは押した瞬間（先行入力に積まない）
//   if (auto a{ buffer.Take(allowNow) }) { /* 行動を始める */ }      // allowNow はキャンセル窓などからゲーム側で作る
//   bool const showPadIcons{ state.LastDevice() == In::Device::Gamepad };

export module Lumina.Input.Adapter;

//****	******	******	******	******	****//

import <cstdint>;

import Lumina.Input;
import Lumina.WinApp.RawInput;

//////	//////	//////	//////	//////	//////

export namespace Lumina::Input::Adapter {
	// GE3 のキー → コアのソース
	constexpr auto Key(WinApp::KEY key_) noexcept -> Source {
		return FromKey(static_cast<std::uint16_t>(static_cast<std::uint32_t>(key_) & 0xFFU));
	}

	// GE3 の RawInput → RawFrame。マウスの差分を取るため、前のフレームの値を覚えておく（1 つの RawInput に 1 つ）
	class RawInputSampler {
	public:
		// out_ を作り直す（前の内容は残さない。パッドの値も消えるので、XInputSampler::Capture はこの後に呼ぶ）。
		// blockKeys_ / blockMouse_ が true なら、キー / マウス（ボタン・移動・ホイール）は何も入力されていない扱いにする。
		// ブロック中もマウスの前回値は更新するので、解除した瞬間に移動量が跳ねない。
		void Capture(WinApp::RawInput const& input_, RawFrame& out_, bool blockKeys_ = false, bool blockMouse_ = false);

		// 次の Capture でマウスの移動量を 0 にする（しばらく Capture していなかったときなど）
		void ResetMouse() noexcept { HasPrev_ = false; }

	private:
		bool HasPrev_{ false };
		std::int64_t PrevX_{ 0 };
		std::int64_t PrevY_{ 0 };
		std::int32_t PrevWheel_{ 0 };
	};

	// マウスで視点を回す間、カーソルを隠してウィンドウ（クライアント領域）の中に閉じ込める。
	// 閉じ込めないと、カーソルがウィンドウの外へ出たままクリックして、別のアプリに切り替わってしまう。
	// 毎フレーム Update を呼ぶ。このアプリのウィンドウが前面にないとき（Alt+Tab など）は自動で外す。
	// ShowCursor は表示カウンタなので、隠す・戻すは 1 回ずつにそろえる（Locked_ で管理）。
	class CursorLock {
	public:
		CursorLock() = default;
		CursorLock(CursorLock const&) = delete;
		auto operator=(CursorLock const&) -> CursorLock& = delete;
		~CursorLock() { Release(); }

		// want_ が true で、ウィンドウが前面にあれば固定する。そうでなければ外す
		void Update(bool want_);
		void Release() noexcept;
		auto Locked() const noexcept -> bool { return Locked_; }

	private:
		bool Locked_{ false };
	};

	// XInput のパッド → RawFrame（パッドの部分だけ書く）
	class XInputSampler {
	public:
		// retryFrames_: パッドを見失っている間、何フレームごとに探し直すか（既定 120 = 60fps で 2 秒）
		explicit XInputSampler(std::uint32_t retryFrames_ = 120U) noexcept : RetryFrames_{ retryFrames_ } {}

		// out_ のパッドのボタンとアナログを書き換える（キー・マウスは触らない）。
		// つながっていない、または block_ が true なら、パッドは何も入力されていない扱いにする
		void Capture(RawFrame& out_, bool block_ = false);

		auto Connected() const noexcept -> bool { return Connected_; }
		auto UserIndex() const noexcept -> std::uint32_t { return UserIndex_; }	// Connected() のときだけ意味がある

		// 次の Capture ですぐ探し直す（設定画面で「パッドを探す」など）
		void Rescan() noexcept { Wait_ = 0U; }

	private:
		std::uint32_t RetryFrames_{ 120U };
		std::uint32_t Wait_{ 0U };			// 0 になったら探す
		std::uint32_t UserIndex_{ 0U };
		bool Connected_{ false };
	};
}
