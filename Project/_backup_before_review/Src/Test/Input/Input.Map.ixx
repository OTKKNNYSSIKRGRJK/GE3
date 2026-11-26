export module Lumina.Input : Map;

//****	******	******	******	******	****//

import <array>;
import <bitset>;
import <concepts>;
import <cstddef>;
import <cstdint>;
import <span>;
import <type_traits>;
import <vector>;

//////	//////	//////	//////	//////	//////

// 生の入力 → 行動
//
// 1 フレームの流れ
//   アダプタ: OS / エンジンの入力 → RawFrame（キー 256 個・マウスボタン・マウス移動量・ホイール・パッドのボタン・スティック・トリガー）
//   コア:     InputState::Update(map, frame) → パッドにデッドゾーン → 行動ごとの Down / Pressed / Released / HeldFrames / 軸の値
//
// キーボードとパッドは同じ行動に両方バインドしてよい（Move に WASD と左スティック、など）。値は足し合わされる。
// 視点は、マウスは「そのフレームに動いた量」、スティックは「倒している間ずっと」の値なので、
// スティックの Scale は「端まで倒したときに 1 フレームで回る量」として決める（固定 60fps 前提。可変 dt にするならスティックだけ dt を掛ける）。
//
// 行動の種類
//   Button  どれか 1 つのバインドが 0.5 以上なら押している（デジタル入力は 0 / 1、アナログは値 × Scale で判定）
//   Axis1D  バインドの値 × Scale を足し合わせた 1 次元の値（ホイールでズーム、など）
//   Axis2D  同じく 2 次元（WASD で移動、マウスで視点、など）。キー 4 つは「W → Y に +1」のように 1 つずつバインドする
//
// 軸の扱い
//   - 逆向きのキーを両方押すと打ち消し合って 0 になる。
//   - ClampToUnit を立てると、Axis2D は長さ 1 以下（斜めが速くならない）、Axis1D は [-1, 1] に収める。マウスの視点には立てない。
//   - 軸の Down は「値が 0 でない」。Pressed / Released もそれに従う（動き始めた / 止まった瞬間）。

export namespace Lumina::Input {
	inline constexpr std::size_t KeyCount{ 256U };			// キーコードは 0..255（GE3 では Windows の仮想キーコード）
	inline constexpr std::size_t MaxActions{ 64U };			// 行動の数の上限（ActionMask が 64bit なので）

	using ActionId = std::uint8_t;
	using ActionMask = std::uint64_t;

	// ゲーム側の行動の enum は underlying type を ActionId（std::uint8_t）にする。
	//   enum class Act : Lumina::Input::ActionId { Move, Attack, ... };
	template<class T>
	concept ActionKey =
		std::same_as<T, ActionId> ||
		(std::is_enum_v<T> && std::same_as<std::underlying_type_t<T>, ActionId>);

	template<ActionKey T>
	constexpr auto ToActionId(T a_) noexcept -> ActionId { return static_cast<ActionId>(a_); }

	template<ActionKey... T>
	constexpr auto MaskOf(T... a_) noexcept -> ActionMask {
		return (ActionMask{ 0U } | ... | (ActionMask{ 1U } << ToActionId(a_)));
	}

	//====	======	======	======	======	====//
	//	生の入力								//
	//====	======	======	======	======	====//

	enum class MouseButton : std::uint8_t { Left, Right, Middle, X1, X2, Count };

	// パッドのボタン（Xbox の呼び名。PlayStation なら A = ×、B = ○、X = □、Y = △、Back = Share、Start = Options）
	enum class GamepadButton : std::uint8_t {
		A, B, X, Y,
		LeftShoulder, RightShoulder,	// LB / RB
		Back, Start,
		LeftThumb, RightThumb,			// スティック押し込み（L3 / R3）
		DpadUp, DpadDown, DpadLeft, DpadRight,
		Count
	};

	enum class Analog : std::uint8_t {
		// マウス: そのフレームのあいだに動いた量
		MouseX,			// 画面右が正（px）
		MouseY,			// 画面下が正（px）
		Wheel,			// 奥へ回すと正（ノッチ数）
		// パッド: 今の倒し具合。アダプタは生の値を正規化して入れるだけで、デッドゾーンはコアが掛ける（GamepadTuning）
		LeftStickX,		// 右が正 [-1, 1]
		LeftStickY,		// 上が正 [-1, 1]（マウスの Y と向きが逆）
		RightStickX,
		RightStickY,
		LeftTrigger,	// [0, 1]
		RightTrigger,
		Count
	};

	// 1 フレームぶんの生の入力。アダプタが毎フレーム作る（前のフレームの値を残さない）
	struct RawFrame {
		std::bitset<KeyCount> Keys{};
		std::bitset<static_cast<std::size_t>(MouseButton::Count)> MouseButtons{};
		std::bitset<static_cast<std::size_t>(GamepadButton::Count)> PadButtons{};
		std::array<float, static_cast<std::size_t>(Analog::Count)> Analogs{};

		void SetKey(std::uint32_t code_, bool down_) noexcept {
			if (code_ < KeyCount) { Keys.set(code_, down_); }
		}
		void SetMouse(MouseButton b_, bool down_) noexcept {
			if (b_ < MouseButton::Count) { MouseButtons.set(static_cast<std::size_t>(b_), down_); }
		}
		void SetPad(GamepadButton b_, bool down_) noexcept {
			if (b_ < GamepadButton::Count) { PadButtons.set(static_cast<std::size_t>(b_), down_); }
		}
		void SetAnalog(Analog a_, float v_) noexcept {
			if (a_ < Analog::Count) { Analogs[static_cast<std::size_t>(a_)] = v_; }
		}
		auto AnalogValue(Analog a_) const noexcept -> float {
			return (a_ < Analog::Count) ? Analogs[static_cast<std::size_t>(a_)] : 0.0f;
		}
	};

	//====	======	======	======	======	====//
	//	パッドのデッドゾーン					//
	//====	======	======	======	======	====//
	// スティックは中心付近でも 0 にならない（個体差あり）ので、小さい倒し具合を捨てる。
	// スティックは X と Y をまとめて円で切る（軸ごとに切ると、斜めに倒したとき上下左右に吸い付く）。
	// 切った後は Inner..Outer を 0..1 に伸ばし直す（Inner を少し超えたところで値が跳ねないように）。
	// 既定値は XInput.h の XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE（7849）/ RIGHT（8689）/ TRIGGER_THRESHOLD（30）を正規化したもの。

	struct StickZone {
		float Inner{ 0.0f };	// これ未満の倒し具合は 0
		float Outer{ 1.0f };	// これ以上は 1（端まで倒しても 1 に届かない個体のため）
	};

	struct GamepadTuning {
		StickZone LeftStick{ 7849.0f / 32767.0f, 1.0f };
		StickZone RightStick{ 8689.0f / 32767.0f, 1.0f };
		float TriggerInner{ 30.0f / 255.0f };
	};

	// 0 <= Inner < Outer <= 1、TriggerInner は [0, 1)
	auto IsValid(GamepadTuning const& t_) noexcept -> bool;

	// スティックの (x, y) に円のデッドゾーンを掛ける。結果の長さは [0, 1]、向きは変えない
	void ShapeStick(float& x_, float& y_, StickZone zone_) noexcept;

	// トリガーの値に [inner, 1] → [0, 1] を掛ける
	auto ShapeTrigger(float v_, float inner_) noexcept -> float;

	// どの機器を最後に触ったか（ボタン表示の切り替え、マウスとスティックで視点の扱いを変える、など）
	enum class Device : std::uint8_t { KeyboardMouse, Gamepad };

	//====	======	======	======	======	====//
	//	バインド								//
	//====	======	======	======	======	====//

	enum class SourceKind : std::uint8_t { None, Key, MouseButton, GamepadButton, Analog };

	// 行動に結びつける入力 1 つ
	struct Source {
		SourceKind Kind{ SourceKind::None };
		std::uint16_t Code{ 0U };
	};

	constexpr auto FromKey(std::uint16_t code_) noexcept -> Source { return { SourceKind::Key, code_ }; }
	constexpr auto FromMouse(MouseButton b_) noexcept -> Source { return { SourceKind::MouseButton, static_cast<std::uint16_t>(b_) }; }
	constexpr auto FromPad(GamepadButton b_) noexcept -> Source { return { SourceKind::GamepadButton, static_cast<std::uint16_t>(b_) }; }
	constexpr auto FromAnalog(Analog a_) noexcept -> Source { return { SourceKind::Analog, static_cast<std::uint16_t>(a_) }; }

	// そのフレームの値。キー・ボタンは 0 / 1、アナログは frame_ の値そのまま（デッドゾーンは掛けない）。不正なソースは 0
	auto ReadSource(RawFrame const& frame_, Source src_) noexcept -> float;

	enum class ActionKind : std::uint8_t { Undefined, Button, Axis1D, Axis2D };
	enum class Component : std::uint8_t { X, Y };

	struct ActionDesc {
		ActionKind Kind{ ActionKind::Undefined };
		bool ClampToUnit{ false };
	};

	struct Binding {
		ActionId Action{ 0U };
		Source Src{};
		Component Comp{ Component::X };
		float Scale{ 1.0f };
	};

	// 行動の定義とバインドの表（データ）。実行時の状態は持たない
	class InputMap {
	public:
		// 行動を定義する。定義し直すと、その行動のバインドは消える。
		// false: id が範囲外、または kind が Undefined
		auto Define(ActionId id_, ActionKind kind_, bool clampToUnit_ = false) -> bool;

		// バインドを足す（同じ行動に何個でも）。
		// false: 行動が未定義 / ソースが不正（範囲外のコードを含む）/ Button・Axis1D に Y を指定 / Scale が有限でない
		auto Bind(ActionId id_, Source src_, Component comp_ = Component::X, float scale_ = 1.0f) -> bool;

		// その行動のバインドだけ消す（定義は残す）
		void Unbind(ActionId id_);

		auto Desc(ActionId id_) const noexcept -> ActionDesc;
		auto Bindings() const noexcept -> std::span<Binding const> { return Bindings_; }

		// パッドのデッドゾーン。false: 値が不正（そのときは変えない）
		auto SetGamepadTuning(GamepadTuning const& t_) noexcept -> bool;
		auto Gamepad() const noexcept -> GamepadTuning const& { return Gamepad_; }

		template<ActionKey A>
		auto Define(A a_, ActionKind kind_, bool clampToUnit_ = false) -> bool { return Define(ToActionId(a_), kind_, clampToUnit_); }
		template<ActionKey A>
		auto Bind(A a_, Source src_, Component comp_ = Component::X, float scale_ = 1.0f) -> bool { return Bind(ToActionId(a_), src_, comp_, scale_); }
		template<ActionKey A>
		void Unbind(A a_) { Unbind(ToActionId(a_)); }
		template<ActionKey A>
		auto Desc(A a_) const noexcept -> ActionDesc { return Desc(ToActionId(a_)); }

	private:
		std::array<ActionDesc, MaxActions> Descs_{};
		std::vector<Binding> Bindings_{};
		GamepadTuning Gamepad_{};
	};

	//====	======	======	======	======	====//
	//	状態									//
	//====	======	======	======	======	====//

	struct ActionState {
		bool Down{ false };
		bool Pressed{ false };					// このフレームで Down になった
		bool Released{ false };					// このフレームで Down でなくなった
		std::uint32_t HeldFrames{ 0U };			// Down の間、押したフレームを 1 として数える。離すと 0
		std::uint32_t ReleasedHeldFrames{ 0U };	// Released のフレームだけ、押していたフレーム数。それ以外は 0
		float X{ 0.0f };						// Button は Down なら 1。軸は値
		float Y{ 0.0f };
	};

	struct Axis2 {
		float X{ 0.0f };
		float Y{ 0.0f };
	};

	// 行動ごとの実行時の状態
	class InputState {
	public:
		// 1 フレームに 1 回。frame_ から行動の値を作り、前のフレームとの差でエッジを作る
		void Update(InputMap const& map_, RawFrame const& frame_);

		// エッジを出さずに、今の入力に合わせる（シーンに入った瞬間など）。
		// 例: タイトルで決定に使った Space を押したまま InGame に入っても、ジャンプの Pressed が出ない
		void Sync(InputMap const& map_, RawFrame const& frame_);

		// 全部離した状態にする（エッジは出さない）。この後も押され続けていれば、次の Update で Pressed が出る
		void Clear() noexcept;

		auto State(ActionId id_) const noexcept -> ActionState const&;
		auto PressedMask() const noexcept -> ActionMask;	// このフレームで Pressed の行動

		// 最後に触った機器。片方だけに入力があったフレームで切り替わる（両方に入力があれば変えない）。
		// パッドはデッドゾーンを掛けた後で見るので、スティックの中心のぶれでは切り替わらない。初期値は KeyboardMouse
		auto LastDevice() const noexcept -> Device { return LastDevice_; }

		template<ActionKey A> auto State(A a_) const noexcept -> ActionState const& { return State(ToActionId(a_)); }
		template<ActionKey A> auto Down(A a_) const noexcept -> bool { return State(a_).Down; }
		template<ActionKey A> auto Pressed(A a_) const noexcept -> bool { return State(a_).Pressed; }
		template<ActionKey A> auto Released(A a_) const noexcept -> bool { return State(a_).Released; }
		template<ActionKey A> auto HeldFrames(A a_) const noexcept -> std::uint32_t { return State(a_).HeldFrames; }
		template<ActionKey A> auto Value(A a_) const noexcept -> float { return State(a_).X; }
		template<ActionKey A> auto Axis(A a_) const noexcept -> Axis2 { auto const& s{ State(a_) }; return { s.X, s.Y }; }

	private:
		void Evaluate(InputMap const& map_, RawFrame const& frame_, bool edges_);

		std::array<ActionState, MaxActions> States_{};
		Device LastDevice_{ Device::KeyboardMouse };
	};
}
