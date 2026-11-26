module Game.PlayerInput;

//****	******	******	******	******	****//

import <cstddef>;
import <cstdint>;
import <optional>;
import <string_view>;

import Lumina.Input;
import Lumina.Input.Adapter;
import Lumina.WinApp.RawInput;

import Game.KeyConfig;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

//////	//////	//////	//////	//////	//////

namespace Game::PlayerInput {
	namespace In = Lumina::Input;
	using In::ActionKind;
	using In::Analog;
	using In::Component;
	using In::GamepadButton;
	using In::MouseButton;
	using Lumina::WinApp::KEY;

	namespace {
		constexpr std::uint16_t VK_Tab{ 0x09U };	// GE3 の KEY に TAB が無いので仮想キーコードで直接
	}

	auto Name(Act a_) noexcept -> std::string_view {
		switch (a_) {
		case Act::Dodge:	return "Dodge";
		case Act::Jump:		return "Jump";
		case Act::Guard:	return "Guard";
		case Act::Attack:	return "Attack";
		case Act::Tool:		return "Tool";
		case Act::UseItem:	return "UseItem";
		case Act::LockOn:	return "LockOn";
		case Act::Move:		return "Move";
		case Act::Look:		return "Look";
		case Act::Zoom:		return "Zoom";
		default:			return "?";
		}
	}

	auto ActFromName(std::string_view name_) noexcept -> std::optional<Act> {
		for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(Act::Count); ++i) {
			Act const a{ static_cast<Act>(i) };
			if (Name(a) == name_) { return a; }
		}
		return std::nullopt;
	}

	auto BuildDefaultMap() -> In::InputMap {
		return BuildMap(Game::KeyConfig::Defaults());
	}

	auto BuildMap(Game::KeyConfig::Bindings const& keys_) -> In::InputMap {
		using In::Adapter::Key;
		using Game::KeyConfig::Slot;
		auto const keyOf{ [&keys_](Slot s_) { return keys_[static_cast<std::size_t>(s_)]; } };
		using In::FromAnalog;
		using In::FromKey;
		using In::FromMouse;
		using In::FromPad;

		In::InputMap m{};

		// ボタン
		m.Define(Act::Dodge, ActionKind::Button);
		m.Bind(Act::Dodge, keyOf(Slot::Dodge));
		m.Bind(Act::Dodge, FromPad(GamepadButton::B));

		m.Define(Act::Jump, ActionKind::Button);
		m.Bind(Act::Jump, keyOf(Slot::Jump));
		m.Bind(Act::Jump, FromPad(GamepadButton::A));

		m.Define(Act::Guard, ActionKind::Button);
		m.Bind(Act::Guard, keyOf(Slot::Guard));
		m.Bind(Act::Guard, FromPad(GamepadButton::LeftShoulder));

		m.Define(Act::Attack, ActionKind::Button);
		m.Bind(Act::Attack, keyOf(Slot::Attack));
		m.Bind(Act::Attack, FromPad(GamepadButton::RightShoulder));

		m.Define(Act::Tool, ActionKind::Button);
		m.Bind(Act::Tool, Key(KEY::CTRL_LEFT));
		m.Bind(Act::Tool, FromAnalog(Analog::RightTrigger));		// 半分以上引いたら押した扱い

		m.Define(Act::UseItem, ActionKind::Button);
		m.Bind(Act::UseItem, Key(KEY::R));
		m.Bind(Act::UseItem, FromPad(GamepadButton::DpadUp));

		m.Define(Act::LockOn, ActionKind::Button);
		m.Bind(Act::LockOn, FromMouse(MouseButton::Middle));		// GE3 の RawInput は記録しないので、今は効かない
		m.Bind(Act::LockOn, FromKey(VK_Tab));
		m.Bind(Act::LockOn, FromPad(GamepadButton::RightThumb));

		// 軸
		m.Define(Act::Move, ActionKind::Axis2D, true);			// 斜めでも長さ 1
		m.Bind(Act::Move, keyOf(Slot::MoveForward), Component::Y, +1.0f);
		m.Bind(Act::Move, keyOf(Slot::MoveBack), Component::Y, -1.0f);
		m.Bind(Act::Move, keyOf(Slot::MoveLeft), Component::X, -1.0f);
		m.Bind(Act::Move, keyOf(Slot::MoveRight), Component::X, +1.0f);
		m.Bind(Act::Move, FromAnalog(Analog::LeftStickX), Component::X);
		m.Bind(Act::Move, FromAnalog(Analog::LeftStickY), Component::Y);

		m.Define(Act::Look, ActionKind::Axis2D);				// 収めない（マウスを速く動かせば大きく回る）
		m.Bind(Act::Look, FromAnalog(Analog::MouseX), Component::X, +MouseLookRadPerCount);
		m.Bind(Act::Look, FromAnalog(Analog::MouseY), Component::Y, -MouseLookRadPerCount);
		m.Bind(Act::Look, FromAnalog(Analog::RightStickX), Component::X, +StickLookRadPerFrame);
		m.Bind(Act::Look, FromAnalog(Analog::RightStickY), Component::Y, +StickLookRadPerFrame);

		m.Define(Act::Zoom, ActionKind::Axis1D);
		m.Bind(Act::Zoom, FromAnalog(Analog::Wheel));

		return m;
	}

	void Controls::Initialize() {
		Map_ = BuildMap(Game::KeyConfig::Current());
		KeyConfigVersion_ = Game::KeyConfig::Version();
		State_.Clear();
		Buffer_.Clear();
		NeedsSync_ = true;
	}

	void Controls::Update(Lumina::WinApp::RawInput const& input_, bool syncOnly_, bool mouseLook_) {
		// 設定画面で割り当てが変わったら作り直す（押しっぱなしのキーで行動が出ないよう、次は Sync から）
		if (std::uint32_t const v{ Game::KeyConfig::Version() }; v != KeyConfigVersion_) {
			Map_ = BuildMap(Game::KeyConfig::Current());
			KeyConfigVersion_ = v;
			NeedsSync_ = true;
		}

		// カーソルの固定（Debug の F1 は ImGui がキーボードを掴んでいても効くよう、RawInput から直接読む）
		bool const f1{ input_.Keyboard().IsPressed(KEY::F1) };
#if defined(_DEBUG)
		if (f1 && !PrevF1_) { DebugFreeCursor_ = !DebugFreeCursor_; }
#endif
		PrevF1_ = f1;
		Cursor_.Update(mouseLook_ && !syncOnly_ && !DebugFreeCursor_);

		bool blockKeys{ false };
		bool blockMouse{ false };
#if defined(_DEBUG)
		// ImGui を操作している間は、ゲームには何も押されていないように見せる。
		// カーソルを固定している間は ImGui にマウスを渡さない（隠れたカーソルが ImGui のウィンドウに重なって、クリックを取られないように）
		ImGuiIO& io{ ImGui::GetIO() };
		if (Cursor_.Locked()) { io.ConfigFlags |= ImGuiConfigFlags_NoMouse; }
		else { io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; }
		blockKeys = io.WantCaptureKeyboard;
		blockMouse = io.WantCaptureMouse && !Cursor_.Locked();
#endif
		if (NeedsSync_) { KeyboardMouse_.ResetMouse(); }	// 前回の移動量の累計が古いので、移動量を 0 から
		KeyboardMouse_.Capture(input_, Frame_, blockKeys, blockMouse);
		Gamepad_.Capture(Frame_);
		if (!Cursor_.Locked()) {
			// 固定していない間は、マウスの動きを視点に使わない（カーソルで ImGui を触っている、ウィンドウが裏にある、など）
			Frame_.SetAnalog(Analog::MouseX, 0.0f);
			Frame_.SetAnalog(Analog::MouseY, 0.0f);
		}

		if (syncOnly_ || NeedsSync_) {
			State_.Sync(Map_, Frame_);
			Buffer_.Clear();
			NeedsSync_ = false;
			return;
		}

		State_.Update(Map_, Frame_);
		Buffer_.Advance(1U);		// ヒットストップを入れたら、その間は 0 にする
		Buffer_.Capture(State_, BufferedActions);
	}

	auto Controls::LookDelta() const noexcept -> In::Axis2 {
		In::Axis2 const a{ State_.Axis(Act::Look) };
		return { a.X * LookSensitivity_, a.Y * LookSensitivity_ };
	}

	void Controls::DrawEditor() {
#if defined(_DEBUG)
		if (!ImGui::Begin("Player Input")) {
			ImGui::End();
			return;
		}

		ImGui::Text("Last device: %s", (State_.LastDevice() == In::Device::Gamepad) ? "Gamepad" : "Keyboard/Mouse");
		ImGui::Text("Gamepad: %s", Gamepad_.Connected() ? "connected" : "not connected");
		ImGui::SameLine();
		if (ImGui::SmallButton("Rescan")) { Gamepad_.Rescan(); }
		ImGui::Text("Cursor: %s  (F1: %s)", Cursor_.Locked() ? "locked" : "free", DebugFreeCursor_ ? "released" : "auto");
		ImGui::SliderFloat("Look sensitivity", &LookSensitivity_, 0.1f, 4.0f, "%.2f");

		if (ImGui::BeginTable("##actions", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("State");
			ImGui::TableSetupColumn("Held");
			ImGui::TableSetupColumn("Value");
			ImGui::TableHeadersRow();
			for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(Act::Count); ++i) {
				Act const a{ static_cast<Act>(i) };
				In::ActionState const& s{ State_.State(a) };
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(Name(a).data());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s.Pressed ? "PRESSED" : s.Released ? "released" : s.Down ? "down" : "-");
				ImGui::TableNextColumn();
				ImGui::Text("%u", s.HeldFrames);
				ImGui::TableNextColumn();
				switch (Map_.Desc(a).Kind) {
				case ActionKind::Axis2D:	ImGui::Text("(%+.3f, %+.3f)", s.X, s.Y); break;
				case ActionKind::Axis1D:	ImGui::Text("%+.3f", s.X); break;
				default:					ImGui::TextUnformatted(""); break;
				}
			}
			ImGui::EndTable();
		}

		ImGui::SeparatorText("Buffer (oldest first)");
		for (In::BufferedInput const& e : Buffer_.Entries()) {
			ImGui::Text("%s  age %u", Name(static_cast<Act>(e.Action)).data(), e.Age);
		}
		if (Buffer_.Entries().empty()) { ImGui::TextDisabled("(empty)"); }

		ImGui::End();
#endif
	}
}
