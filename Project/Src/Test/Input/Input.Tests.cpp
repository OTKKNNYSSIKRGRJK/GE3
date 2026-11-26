module Lumina.Input.Tests;

//****	******	******	******	******	****//

import <cmath>;
import <cstddef>;
import <cstdint>;
import <format>;
import <optional>;
import <string>;
import <string_view>;
import <vector>;

import Lumina.Input;

//////	//////	//////	//////	//////	//////

namespace Lumina::Input::Tests {
	namespace {
		class Context {
		public:
			Context(std::vector<Failure>& out_, std::string_view name_) : Out_{ out_ }, Name_{ name_ } {}

			void Check(bool ok_, std::string_view what_) {
				if (!ok_) { Out_.push_back(Failure{ std::string{ Name_ }, std::string{ what_ } }); }
			}

		private:
			std::vector<Failure>& Out_;
			std::string_view Name_;
		};

		auto Near(float a_, float b_, float eps_ = 1e-5f) -> bool { return std::fabs(a_ - b_) <= eps_; }

		// テスト用の行動（ゲーム側の enum のつもり）
		enum class Act : ActionId { Move, Look, Zoom, Jump, Dodge, Attack, Guard, WheelNext, Count };

		// テスト用のキーコード（Windows の仮想キーコードの値。コアにとっては 0..255 のただの番号）
		constexpr std::uint16_t K_W{ 0x57 };
		constexpr std::uint16_t K_A{ 0x41 };
		constexpr std::uint16_t K_S{ 0x53 };
		constexpr std::uint16_t K_D{ 0x44 };
		constexpr std::uint16_t K_Space{ 0x20 };
		constexpr std::uint16_t K_Enter{ 0x0D };
		constexpr std::uint16_t K_ShiftL{ 0xA0 };

		auto MakeMap() -> InputMap {
			InputMap m{};
			m.Define(Act::Move, ActionKind::Axis2D, true);
			m.Bind(Act::Move, FromKey(K_W), Component::Y, +1.0f);
			m.Bind(Act::Move, FromKey(K_S), Component::Y, -1.0f);
			m.Bind(Act::Move, FromKey(K_A), Component::X, -1.0f);
			m.Bind(Act::Move, FromKey(K_D), Component::X, +1.0f);
			m.Define(Act::Look, ActionKind::Axis2D);
			m.Bind(Act::Look, FromAnalog(Analog::MouseX), Component::X, 0.1f);
			m.Bind(Act::Look, FromAnalog(Analog::MouseY), Component::Y, -0.1f);
			m.Define(Act::Zoom, ActionKind::Axis1D, true);
			m.Bind(Act::Zoom, FromAnalog(Analog::Wheel));
			m.Define(Act::Jump, ActionKind::Button);
			m.Bind(Act::Jump, FromKey(K_Space));
			m.Bind(Act::Jump, FromKey(K_Enter));
			m.Define(Act::Dodge, ActionKind::Button);
			m.Bind(Act::Dodge, FromKey(K_ShiftL));
			m.Define(Act::Attack, ActionKind::Button);
			m.Bind(Act::Attack, FromMouse(MouseButton::Left));
			m.Define(Act::Guard, ActionKind::Button);
			m.Bind(Act::Guard, FromMouse(MouseButton::Right));
			m.Define(Act::WheelNext, ActionKind::Button);
			m.Bind(Act::WheelNext, FromAnalog(Analog::Wheel), Component::X, 1.0f);
			return m;
		}

		template<class... C>
		auto Keys(C... codes_) -> RawFrame {
			RawFrame f{};
			(f.SetKey(codes_, true), ...);
			return f;
		}

		//====	======	======	======	======	====//
		//	:Map									//
		//====	======	======	======	======	====//

		void MapValidation(std::vector<Failure>& out_) {
			Context c{ out_, "Map.Validation" };
			InputMap m{};
			c.Check(!m.Define(static_cast<ActionId>(MaxActions), ActionKind::Button), "範囲外の id は定義できない");
			c.Check(!m.Define(Act::Jump, ActionKind::Undefined), "Undefined では定義できない");
			c.Check(!m.Bind(Act::Jump, FromKey(K_Space)), "未定義の行動にはバインドできない");

			m.Define(Act::Jump, ActionKind::Button);
			c.Check(m.Bind(Act::Jump, FromKey(K_Space)), "正しいバインド");
			c.Check(!m.Bind(Act::Jump, FromKey(256U)), "範囲外のキーコード");
			c.Check(!m.Bind(Act::Jump, Source{ SourceKind::MouseButton, 5U }), "範囲外のマウスボタン");
			c.Check(!m.Bind(Act::Jump, Source{ SourceKind::Analog, static_cast<std::uint16_t>(Analog::Count) }), "範囲外のアナログ");
			c.Check(!m.Bind(Act::Jump, Source{ SourceKind::GamepadButton, static_cast<std::uint16_t>(GamepadButton::Count) }), "範囲外のパッドのボタン");
			c.Check(m.Bind(Act::Jump, FromPad(GamepadButton::A)), "パッドのボタン");
			c.Check(!m.Bind(Act::Jump, Source{}), "None のソース");
			c.Check(!m.Bind(Act::Jump, FromKey(K_Enter), Component::Y), "Button に Y は指定できない");
			c.Check(!m.Bind(Act::Jump, FromKey(K_Enter), Component::X, std::nanf("")), "NaN の Scale");

			m.Define(Act::Zoom, ActionKind::Axis1D);
			c.Check(!m.Bind(Act::Zoom, FromAnalog(Analog::Wheel), Component::Y), "Axis1D に Y は指定できない");
			m.Define(Act::Move, ActionKind::Axis2D);
			c.Check(m.Bind(Act::Move, FromKey(K_W), Component::Y), "Axis2D には Y を指定できる");

			c.Check(m.Bindings().size() == 3U, "バインドは 3 件（Jump の Space・パッドの A と Move の W）");
			m.Define(Act::Jump, ActionKind::Button);
			c.Check(m.Bindings().size() == 1U, "定義し直すとその行動のバインドが消える");
			m.Unbind(Act::Move);
			c.Check(m.Bindings().empty() && m.Desc(Act::Move).Kind == ActionKind::Axis2D, "Unbind はバインドだけ消して定義は残す");
		}

		// 押した瞬間・押している間・離した瞬間
		void StateButtonEdges(std::vector<Failure>& out_) {
			Context c{ out_, "State.ButtonEdges" };
			InputMap const m{ MakeMap() };
			InputState s{};

			s.Update(m, Keys());
			c.Check(!s.Down(Act::Jump) && !s.Pressed(Act::Jump) && !s.Released(Act::Jump), "押していない");

			for (std::uint32_t i = 1; i <= 3; ++i) {
				s.Update(m, Keys(K_Space));
				c.Check(s.Down(Act::Jump), std::format("{} フレーム目: Down", i));
				c.Check(s.Pressed(Act::Jump) == (i == 1U), std::format("{} フレーム目: Pressed は最初だけ", i));
				c.Check(s.HeldFrames(Act::Jump) == i, std::format("{} フレーム目: HeldFrames = {}", i, s.HeldFrames(Act::Jump)));
				c.Check(s.Value(Act::Jump) == 1.0f, "Button の値は 1");
			}

			s.Update(m, Keys());
			c.Check(s.Released(Act::Jump) && !s.Down(Act::Jump), "離したフレームで Released");
			c.Check(s.State(Act::Jump).ReleasedHeldFrames == 3U, "Released のフレームで押していた長さ = 3");
			c.Check(s.HeldFrames(Act::Jump) == 0U && s.Value(Act::Jump) == 0.0f, "離したら HeldFrames と値は 0");

			s.Update(m, Keys());
			c.Check(!s.Released(Act::Jump) && s.State(Act::Jump).ReleasedHeldFrames == 0U, "Released は 1 フレームだけ");
		}

		// 同じ行動に複数のキー: どれか 1 つで押している。持ち替えでエッジは出ない
		void StateMultiSource(std::vector<Failure>& out_) {
			Context c{ out_, "State.MultiSource" };
			InputMap const m{ MakeMap() };
			InputState s{};

			s.Update(m, Keys(K_Space));
			c.Check(s.Pressed(Act::Jump), "Space で Pressed");
			s.Update(m, Keys(K_Space, K_Enter));
			c.Check(!s.Pressed(Act::Jump) && s.Value(Act::Jump) == 1.0f, "Enter を足しても Pressed は出ず、値は 1 のまま");
			s.Update(m, Keys(K_Enter));
			c.Check(s.Down(Act::Jump) && !s.Released(Act::Jump), "Space を離しても Enter で押したまま");
			c.Check(s.HeldFrames(Act::Jump) == 3U, "持ち替えても押している長さは続く");
			s.Update(m, Keys());
			c.Check(s.Released(Act::Jump), "全部離して Released");
		}

		// キー 4 つで 2 次元の軸
		void StateAxisComposite(std::vector<Failure>& out_) {
			Context c{ out_, "State.AxisComposite" };
			InputMap const m{ MakeMap() };
			InputState s{};

			s.Update(m, Keys(K_W));
			auto a{ s.Axis(Act::Move) };
			c.Check(a.X == 0.0f && a.Y == 1.0f && s.Pressed(Act::Move), "W: (0, 1)、動き始めで Pressed");

			s.Update(m, Keys(K_W, K_D));
			a = s.Axis(Act::Move);
			c.Check(Near(a.X, 0.70710678f) && Near(a.Y, 0.70710678f), std::format("W+D は長さ 1 に収まる: ({}, {})", a.X, a.Y));
			c.Check(!s.Pressed(Act::Move), "向きが変わっても Pressed は出ない");

			s.Update(m, Keys(K_W, K_S));
			a = s.Axis(Act::Move);
			c.Check(a.X == 0.0f && a.Y == 0.0f && !s.Down(Act::Move) && s.Released(Act::Move), "W+S は打ち消して 0、止まったので Released");

			InputMap m2{ MakeMap() };
			m2.Define(Act::Move, ActionKind::Axis2D, false);
			m2.Bind(Act::Move, FromKey(K_W), Component::Y);
			m2.Bind(Act::Move, FromKey(K_D), Component::X);
			InputState s2{};
			s2.Update(m2, Keys(K_W, K_D));
			a = s2.Axis(Act::Move);
			c.Check(a.X == 1.0f && a.Y == 1.0f, "ClampToUnit なしなら (1, 1)");
		}

		// マウスの移動量（アナログ）。収めない
		void StateAnalog(std::vector<Failure>& out_) {
			Context c{ out_, "State.Analog" };
			InputMap m{ MakeMap() };
			InputState s{};

			RawFrame f{};
			f.SetAnalog(Analog::MouseX, 30.0f);
			f.SetAnalog(Analog::MouseY, -12.0f);
			s.Update(m, f);
			auto a{ s.Axis(Act::Look) };
			c.Check(Near(a.X, 3.0f) && Near(a.Y, 1.2f), std::format("Scale を掛けるだけで収めない: ({}, {})", a.X, a.Y));

			s.Update(m, RawFrame{});
			c.Check(!s.Down(Act::Look) && s.Released(Act::Look), "動かなければ 0（前のフレームの値は残さない）");

			// アナログとキーの足し合わせ
			m.Bind(Act::Look, FromKey(K_D), Component::X, 1.0f);
			RawFrame g{ Keys(K_D) };
			g.SetAnalog(Analog::MouseX, 10.0f);
			s.Update(m, g);
			c.Check(Near(s.Axis(Act::Look).X, 2.0f), "マウス 1 + キー 1 = 2");
		}

		// 1 次元の軸を [-1, 1] に収める。アナログを Button にする
		void StateAxis1DAndAnalogButton(std::vector<Failure>& out_) {
			Context c{ out_, "State.Axis1DAndAnalogButton" };
			InputMap const m{ MakeMap() };
			InputState s{};

			RawFrame f{};
			f.SetAnalog(Analog::Wheel, 3.0f);
			s.Update(m, f);
			c.Check(s.Value(Act::Zoom) == 1.0f, "Axis1D は 1 に収まる");
			c.Check(s.Pressed(Act::WheelNext), "ホイール 1 ノッチ以上で Button が押される");

			f.SetAnalog(Analog::Wheel, -2.0f);
			s.Update(m, f);
			c.Check(s.Value(Act::Zoom) == -1.0f, "逆向きは -1");
			c.Check(s.Released(Act::WheelNext), "逆向きは 0.5 未満なので離した扱い");
		}

		// Sync: 押しっぱなしで始める
		void StateSync(std::vector<Failure>& out_) {
			Context c{ out_, "State.Sync" };
			InputMap const m{ MakeMap() };
			InputState s{};

			s.Sync(m, Keys(K_Space));
			c.Check(s.Down(Act::Jump) && !s.Pressed(Act::Jump), "Sync は Down にするが Pressed は出さない");
			c.Check(s.HeldFrames(Act::Jump) == 0U, "Sync で押しっぱなし扱いにしたものは 0 から数える");

			s.Update(m, Keys(K_Space));
			c.Check(!s.Pressed(Act::Jump) && s.HeldFrames(Act::Jump) == 1U, "押し続けても Pressed は出ない");

			s.Update(m, Keys());
			c.Check(s.Released(Act::Jump), "離せば Released");

			s.Update(m, Keys());
			s.Sync(m, Keys());
			s.Update(m, Keys(K_Space));
			c.Check(s.Pressed(Act::Jump), "押し直せば Pressed");

			s.Sync(m, Keys());
			c.Check(!s.Down(Act::Jump) && !s.Released(Act::Jump), "Sync で離した扱いにしても Released は出ない");
		}

		// Clear: 全部離した扱い。押され続けていれば次の Update で Pressed
		void StateClear(std::vector<Failure>& out_) {
			Context c{ out_, "State.Clear" };
			InputMap const m{ MakeMap() };
			InputState s{};

			s.Update(m, Keys(K_Space));
			s.Clear();
			c.Check(!s.Down(Act::Jump) && !s.Pressed(Act::Jump) && !s.Released(Act::Jump), "Clear の直後は何も出ない");
			s.Update(m, Keys(K_Space));
			c.Check(s.Pressed(Act::Jump), "押され続けていれば次の Update で Pressed");
		}

		// 未定義の行動・範囲外の id
		void StateUndefined(std::vector<Failure>& out_) {
			Context c{ out_, "State.Undefined" };
			InputMap const m{ MakeMap() };
			InputState s{};
			s.Update(m, Keys(K_Space, K_W));
			c.Check(!s.Down(static_cast<ActionId>(40)), "未定義の行動は押されない");
			c.Check(!s.Down(static_cast<ActionId>(200)) && s.State(static_cast<ActionId>(200)).X == 0.0f, "範囲外の id は空の状態");
		}

		void StatePressedMask(std::vector<Failure>& out_) {
			Context c{ out_, "State.PressedMask" };
			InputMap const m{ MakeMap() };
			InputState s{};

			RawFrame f{ Keys(K_Space, K_ShiftL) };
			f.SetMouse(MouseButton::Left, true);
			s.Update(m, f);
			c.Check(s.PressedMask() == MaskOf(Act::Jump, Act::Dodge, Act::Attack), "このフレームで押した 3 つ");
			s.Update(m, f);
			c.Check(s.PressedMask() == 0U, "押し続けると空");
		}

		//====	======	======	======	======	====//
		//	パッド									//
		//====	======	======	======	======	====//

		// スティックの円のデッドゾーン
		void PadShapeStick(std::vector<Failure>& out_) {
			Context c{ out_, "Pad.ShapeStick" };
			StickZone const z{ 0.25f, 1.0f };
			auto shape{ [&](float x_, float y_, StickZone zone_) { ShapeStick(x_, y_, zone_); return Axis2{ x_, y_ }; } };

			auto a{ shape(0.2f, 0.0f, z) };
			c.Check(a.X == 0.0f && a.Y == 0.0f, "デッドゾーンの内側は 0");
			a = shape(0.25f, 0.0f, z);
			c.Check(a.X == 0.0f, "ちょうど Inner も 0");
			a = shape(1.0f, 0.0f, z);
			c.Check(Near(a.X, 1.0f) && a.Y == 0.0f, "端まで倒すと 1");
			a = shape(0.625f, 0.0f, z);
			c.Check(Near(a.X, 0.5f), std::format("Inner..1 を 0..1 に伸ばす: 0.625 → {}", a.X));
			a = shape(0.0f, -1.0f, z);
			c.Check(a.X == 0.0f && Near(a.Y, -1.0f), "負の向きもそのまま");
			a = shape(1.0f, 1.0f, z);
			c.Check(Near(a.X, 0.70710678f) && Near(a.Y, 0.70710678f), "角（長さ √2）は長さ 1 に収まり、向きは 45°");
			a = shape(0.2f, 0.2f, z);
			c.Check(a.X > 0.0f && Near(a.X, a.Y), "斜めに少し倒すと、軸ごとならデッドゾーン内でも円なら残り、向きも保つ");
			a = shape(0.9f, 0.0f, StickZone{ 0.25f, 0.9f });
			c.Check(Near(a.X, 1.0f), "Outer で 1 に届く");
			a = shape(std::nanf(""), 0.5f, z);
			c.Check(a.X == 0.0f && a.Y == 0.0f, "NaN は 0");
		}

		void PadShapeTrigger(std::vector<Failure>& out_) {
			Context c{ out_, "Pad.ShapeTrigger" };
			c.Check(ShapeTrigger(0.05f, 0.1f) == 0.0f, "Inner 未満は 0");
			c.Check(Near(ShapeTrigger(0.55f, 0.1f), 0.5f), "Inner..1 を 0..1 に伸ばす");
			c.Check(Near(ShapeTrigger(1.0f, 0.1f), 1.0f), "引き切ると 1");
			c.Check(ShapeTrigger(std::nanf(""), 0.1f) == 0.0f, "NaN は 0");
		}

		void PadTuning(std::vector<Failure>& out_) {
			Context c{ out_, "Pad.Tuning" };
			c.Check(IsValid(GamepadTuning{}), "既定値は正しい");
			GamepadTuning t{};
			c.Check(Near(t.LeftStick.Inner, 0.23953f, 1e-4f) && Near(t.TriggerInner, 0.11765f, 1e-4f), "既定値は XInput の推奨値（7849/32767, 30/255）");

			InputMap m{};
			t.LeftStick = StickZone{ -0.1f, 1.0f };
			c.Check(!m.SetGamepadTuning(t), "Inner が負");
			t.LeftStick = StickZone{ 0.5f, 0.5f };
			c.Check(!m.SetGamepadTuning(t), "Inner = Outer");
			t.LeftStick = StickZone{ 0.1f, 1.5f };
			c.Check(!m.SetGamepadTuning(t), "Outer が 1 を超える");
			t.LeftStick = StickZone{ 0.1f, 1.0f };
			t.TriggerInner = 1.0f;
			c.Check(!m.SetGamepadTuning(t), "TriggerInner = 1");
			c.Check(Near(m.Gamepad().LeftStick.Inner, 0.23953f, 1e-4f), "失敗したら変えない");
			t.TriggerInner = 0.2f;
			c.Check(m.SetGamepadTuning(t) && m.Gamepad().LeftStick.Inner == 0.1f, "正しい値は入る");
		}

		auto MakePadMap() -> InputMap {
			InputMap m{};
			m.Define(Act::Move, ActionKind::Axis2D, true);
			m.Bind(Act::Move, FromKey(K_D), Component::X, +1.0f);
			m.Bind(Act::Move, FromAnalog(Analog::LeftStickX), Component::X);
			m.Bind(Act::Move, FromAnalog(Analog::LeftStickY), Component::Y);
			m.Define(Act::Look, ActionKind::Axis2D);
			m.Bind(Act::Look, FromAnalog(Analog::RightStickX), Component::X, 0.05f);
			m.Define(Act::Attack, ActionKind::Button);
			m.Bind(Act::Attack, FromMouse(MouseButton::Left));
			m.Bind(Act::Attack, FromPad(GamepadButton::RightShoulder));
			m.Define(Act::Guard, ActionKind::Button);
			m.Bind(Act::Guard, FromAnalog(Analog::RightTrigger));
			return m;
		}

		auto Pad(float lx_, float ly_) -> RawFrame {
			RawFrame f{};
			f.SetAnalog(Analog::LeftStickX, lx_);
			f.SetAnalog(Analog::LeftStickY, ly_);
			return f;
		}

		// パッドのボタン・スティック・トリガーを行動にバインドする
		void PadBindings(std::vector<Failure>& out_) {
			Context c{ out_, "Pad.Bindings" };
			InputMap const m{ MakePadMap() };
			InputState s{};

			s.Update(m, Pad(0.1f, 0.0f));
			c.Check(!s.Down(Act::Move), "中心のぶれ（0.1）はデッドゾーンで消える");

			s.Update(m, Pad(0.0f, 1.0f));
			c.Check(s.Pressed(Act::Move) && Near(s.Axis(Act::Move).Y, 1.0f), "上に倒し切ると Y = 1");

			RawFrame f{ Pad(1.0f, 0.0f) };
			f.SetKey(K_D, true);
			s.Update(m, f);
			c.Check(Near(s.Axis(Act::Move).X, 1.0f), "スティックとキーを足しても長さ 1 に収まる");

			RawFrame g{};
			g.SetAnalog(Analog::RightStickX, 1.0f);
			s.Update(m, g);
			c.Check(Near(s.Axis(Act::Look).X, 0.05f), "右スティックを倒し切ると Scale そのまま（1 フレームあたりの量）");
			s.Update(m, g);
			c.Check(Near(s.Axis(Act::Look).X, 0.05f), "倒している間は毎フレーム同じ値（マウスと違い、動かさなくても 0 にならない）");

			RawFrame p{};
			p.SetPad(GamepadButton::RightShoulder, true);
			s.Update(m, p);
			c.Check(s.Pressed(Act::Attack), "RB で Pressed");

			RawFrame t{};
			t.SetAnalog(Analog::RightTrigger, 0.4f);
			s.Update(m, t);
			c.Check(!s.Down(Act::Guard), "トリガー 0.4 はデッドゾーンを引くと 0.5 未満なので押していない");
			t.SetAnalog(Analog::RightTrigger, 0.7f);
			s.Update(m, t);
			c.Check(s.Pressed(Act::Guard), "トリガー 0.7 で押した");
		}

		// 最後に触った機器
		void PadLastDevice(std::vector<Failure>& out_) {
			Context c{ out_, "Pad.LastDevice" };
			InputMap const m{ MakePadMap() };
			InputState s{};

			c.Check(s.LastDevice() == Device::KeyboardMouse, "初期値はキーボード・マウス");
			s.Update(m, Pad(0.1f, 0.0f));
			c.Check(s.LastDevice() == Device::KeyboardMouse, "デッドゾーン内のぶれでは切り替わらない");

			RawFrame p{};
			p.SetPad(GamepadButton::A, true);
			s.Update(m, p);
			c.Check(s.LastDevice() == Device::Gamepad, "パッドのボタンで切り替わる（バインドされていないボタンでも）");

			s.Update(m, RawFrame{});
			c.Check(s.LastDevice() == Device::Gamepad, "何も触らなければそのまま");

			RawFrame mouse{};
			mouse.SetAnalog(Analog::MouseX, 3.0f);
			s.Update(m, mouse);
			c.Check(s.LastDevice() == Device::KeyboardMouse, "マウスを動かすと切り替わる");

			RawFrame both{ Pad(1.0f, 0.0f) };
			both.SetKey(K_D, true);
			s.Update(m, both);
			c.Check(s.LastDevice() == Device::KeyboardMouse, "両方に入力があれば変えない（キーボード・マウスのまま）");
			s.Update(m, Pad(1.0f, 0.0f));
			s.Update(m, both);
			c.Check(s.LastDevice() == Device::Gamepad, "両方に入力があれば変えない（パッドのまま）");

			s.Update(m, Pad(1.0f, 0.0f));
			s.Clear();
			c.Check(s.LastDevice() == Device::Gamepad, "Clear しても残す");

			s.Sync(m, Keys(K_D));
			c.Check(s.LastDevice() == Device::KeyboardMouse, "Sync でも切り替わる");
		}

		//====	======	======	======	======	====//
		//	:Buffer									//
		//====	======	======	======	======	====//

		// 押したフレームを含めて Lifetime + 1 フレーム有効
		void BufferLifetime(std::vector<Failure>& out_) {
			Context c{ out_, "Buffer.Lifetime" };
			InputBuffer b{ 3U };
			b.Push(Act::Attack);
			for (int i = 0; i < 3; ++i) { b.Advance(); }
			c.Check(b.Peek(MaskOf(Act::Attack)).has_value(), "年齢 3（= 寿命）ではまだ残る");
			b.Advance();
			c.Check(!b.Peek(MaskOf(Act::Attack)).has_value() && b.Entries().empty(), "年齢 4 で捨てる");

			InputBuffer z{ 0U };
			z.Push(Act::Attack);
			c.Check(z.Peek(MaskOf(Act::Attack)).has_value(), "寿命 0 でも押したフレームは有効");
			z.Advance();
			c.Check(z.Entries().empty(), "寿命 0 は次のフレームで捨てる");

			InputBuffer h{ 3U };
			h.Push(Act::Attack);
			h.Advance(0U);
			h.Advance(0U);
			c.Check(!h.Entries().empty() && h.Entries()[0].Age == 0U, "Advance(0) では年を取らない（ヒットストップ）");
			h.Advance(1000000U);
			c.Check(h.Entries().empty(), "大きく進めたら捨てる");
		}

		// いちばん新しいものを取り、それと同時か前のものは捨てる
		void BufferTakeNewest(std::vector<Failure>& out_) {
			Context c{ out_, "Buffer.TakeNewest" };
			ActionMask const all{ MaskOf(Act::Attack, Act::Dodge, Act::Jump) };

			InputBuffer b{ 10U };
			b.Push(Act::Attack);
			b.Advance();
			b.Push(Act::Attack);
			b.Advance();
			b.Push(Act::Dodge);
			auto t{ b.Take(all) };
			c.Check(t == ToActionId(Act::Dodge), "いちばん新しい Dodge");
			c.Check(b.Entries().empty(), "前に押した Attack 2 つは捨てる");

			// 許可されていないものは飛ばす。新しいものは残る
			InputBuffer d{ 10U };
			d.Push(Act::Attack);
			d.Advance();
			d.Push(Act::Dodge);
			t = d.Take(MaskOf(Act::Attack));
			c.Check(t == ToActionId(Act::Attack), "Attack だけ許可なら Attack");
			c.Check(d.Entries().size() == 1U && d.Entries()[0].Action == ToActionId(Act::Dodge), "後に押した Dodge は残る");
			c.Check(!d.Take(MaskOf(Act::Attack)).has_value(), "Attack はもう無い");

			// 同じフレームなら ActionId の小さい方（enum の並び = 優先順）
			InputBuffer s{ 10U };
			s.Push(Act::Attack);
			s.Push(Act::Dodge);
			t = s.Take(all);
			c.Check(t == ToActionId(Act::Dodge), "同じフレームは id の小さい Dodge（Dodge < Attack）");
			c.Check(s.Entries().empty(), "同時に押した Attack も捨てる");

			InputBuffer e{ 10U };
			c.Check(!e.Take(all).has_value(), "空なら nullopt");
			e.Push(Act::Jump);
			c.Check(e.Peek(all).has_value() && e.Entries().size() == 1U, "Peek は取り出さない");
		}

		// 容量を超えたら古い順に捨てる。範囲外の id は積まない
		void BufferCapacityLimit(std::vector<Failure>& out_) {
			Context c{ out_, "Buffer.Capacity" };
			InputBuffer b{ 100U };
			for (std::size_t i = 0; i < BufferCapacity + 4U; ++i) {
				b.Push((i < 4U) ? Act::Jump : Act::Attack);
				b.Advance();
			}
			c.Check(b.Entries().size() == BufferCapacity, "容量で頭打ち");
			c.Check(!b.Peek(MaskOf(Act::Jump)).has_value(), "最初に積んだ 4 つ（Jump）が捨てられている");
			c.Check(b.Entries().back().Age == 1U && b.Entries().front().Age == BufferCapacity, "古い順に並んでいる");

			InputBuffer o{ 10U };
			o.Push(static_cast<ActionId>(MaxActions));
			c.Check(o.Entries().empty(), "範囲外の id は積まない");
		}

		// Capture は Pressed かつ mask のものを積む
		void BufferCapture(std::vector<Failure>& out_) {
			Context c{ out_, "Buffer.Capture" };
			InputMap const m{ MakeMap() };
			InputState s{};
			InputBuffer b{ 10U };

			RawFrame f{ Keys(K_Space, K_ShiftL) };
			f.SetMouse(MouseButton::Left, true);
			f.SetMouse(MouseButton::Right, true);
			s.Update(m, f);
			b.Capture(s, MaskOf(Act::Attack, Act::Dodge, Act::Jump));
			c.Check(b.Entries().size() == 3U, "Guard は mask に無いので積まない");
			c.Check(b.Entries()[0].Action == ToActionId(Act::Jump) && b.Entries()[2].Action == ToActionId(Act::Attack), "id の小さい順に積む");

			s.Update(m, f);
			b.Advance();
			b.Capture(s, MaskOf(Act::Attack, Act::Dodge, Act::Jump));
			c.Check(b.Entries().size() == 3U, "押し続けても積まない（Pressed だけ）");
		}

		// 引数（枠の番号）は押した時点の値のまま残り、選び方・捨て方は引数に関係ない
		void BufferArgument(std::vector<Failure>& out_) {
			Context c{ out_, "Buffer.Argument" };
			InputBuffer b{ 10U };
			b.Push(Act::Attack, 3U);
			b.Advance();
			b.Push(Act::Attack, 5U);
			auto const p{ b.PeekEntry(MaskOf(Act::Attack)) };
			c.Check(p.has_value() && p->Arg == 5U && p->Age == 0U, "Peek はいちばん新しいものの引数");
			c.Check(b.Entries().size() == 2U, "PeekEntry は取り出さない");
			auto const t{ b.TakeEntry(MaskOf(Act::Attack)) };
			c.Check(t.has_value() && t->Action == ToActionId(Act::Attack) && t->Arg == 5U, "TakeEntry は引数ごと返す");
			c.Check(b.Entries().empty(), "前に押した方（引数 3）も捨てる");

			InputBuffer d{ 10U };
			d.Push(Act::Jump);
			c.Check(d.Entries().size() == 1U && d.Entries()[0].Arg == 0U, "引数なしの Push は 0");
			d.Push(Act::Dodge, 7U);
			c.Check(d.Take(MaskOf(Act::Dodge)) == ToActionId(Act::Dodge), "Take は行動だけを返す（今までどおり）");
			c.Check(!d.TakeEntry(MaskOf(Act::Attack)).has_value(), "許可された行動が無ければ nullopt");
		}

		// Drop は指定した行動だけを捨てる（前に押した別の行動は残す）
		void BufferDrop(std::vector<Failure>& out_) {
			Context c{ out_, "Buffer.Drop" };
			InputBuffer b{ 10U };
			b.Push(Act::Attack);
			b.Advance();
			b.Push(Act::Jump);
			b.Push(Act::Dodge);
			b.Advance();
			b.Push(Act::Jump);
			b.Drop(MaskOf(Act::Jump));
			c.Check(b.Entries().size() == 2U, "Jump は 2 つとも捨てる");
			c.Check(b.Peek(MaskOf(Act::Attack)).has_value() && b.Peek(MaskOf(Act::Dodge)).has_value(), "前に押した Attack と、同時に押した Dodge は残る");
			b.Drop(0U);
			c.Check(b.Entries().size() == 2U, "空のマスクでは何もしない");
		}

		// 使い方の通し: 攻撃の振りの途中で押した次の攻撃が、受付が開いた瞬間に出る。早すぎた押下は消える
		void BufferComboScenario(std::vector<Failure>& out_) {
			Context c{ out_, "Buffer.ComboScenario" };
			InputMap const m{ MakeMap() };
			InputState s{};
			InputBuffer b{ 10U };

			// フレーム 0 に攻撃を押し、受付は 8 フレーム目に開く → 年齢 8 で取り出せる
			auto run{ [&](int pressFrame_, int windowOpen_) -> std::optional<int> {
				s.Clear();
				b.Clear();
				for (int f = 0; f < 40; ++f) {
					RawFrame raw{};
					raw.SetMouse(MouseButton::Left, f == pressFrame_);
					s.Update(m, raw);
					b.Advance();
					b.Capture(s, MaskOf(Act::Attack, Act::Dodge, Act::Jump));
					ActionMask const allow{ (f >= windowOpen_) ? MaskOf(Act::Attack) : ActionMask{ 0U } };
					if (b.Take(allow)) { return f; }
				}
				return std::nullopt;
			} };

			c.Check(run(0, 8) == 8, "10 フレーム前までの押下は、受付が開いたフレームで出る");
			c.Check(run(0, 10) == 10, "ちょうど寿命の 10 フレーム前も出る");
			c.Check(!run(0, 11).has_value(), "11 フレーム前の押下は寿命切れで出ない");
			c.Check(run(12, 8) == 12, "受付が開いた後の押下はその場で出る");
		}
	}

	auto RunAll() -> std::vector<Failure> {
		std::vector<Failure> out{};
		MapValidation(out);
		StateButtonEdges(out);
		StateMultiSource(out);
		StateAxisComposite(out);
		StateAnalog(out);
		StateAxis1DAndAnalogButton(out);
		StateSync(out);
		StateClear(out);
		StateUndefined(out);
		StatePressedMask(out);
		PadShapeStick(out);
		PadShapeTrigger(out);
		PadTuning(out);
		PadBindings(out);
		PadLastDevice(out);
		BufferLifetime(out);
		BufferTakeNewest(out);
		BufferCapacityLimit(out);
		BufferCapture(out);
		BufferComboScenario(out);
		BufferArgument(out);
		BufferDrop(out);
		return out;
	}
}
