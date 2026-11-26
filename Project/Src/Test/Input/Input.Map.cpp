module Lumina.Input;

//****	******	******	******	******	****//

import <algorithm>;
import <array>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <vector>;

//////	//////	//////	//////	//////	//////

namespace Lumina::Input {
	namespace {
		// Button のバインド 1 つが「押している」とみなす値
		constexpr float ButtonThreshold{ 0.5f };

		auto IsValidSource(Source src_) noexcept -> bool {
			switch (src_.Kind) {
			case SourceKind::Key:			return src_.Code < KeyCount;
			case SourceKind::MouseButton:	return src_.Code < static_cast<std::uint16_t>(MouseButton::Count);
			case SourceKind::GamepadButton:	return src_.Code < static_cast<std::uint16_t>(GamepadButton::Count);
			case SourceKind::Analog:		return src_.Code < static_cast<std::uint16_t>(Analog::Count);
			default:						return false;
			}
		}

		auto IsValid(StickZone z_) noexcept -> bool {
			return std::isfinite(z_.Inner) && std::isfinite(z_.Outer) && z_.Inner >= 0.0f && z_.Inner < z_.Outer && z_.Outer <= 1.0f;
		}

		// パッドのアナログにデッドゾーンを掛けたフレームを作る
		auto ShapePad(RawFrame const& frame_, GamepadTuning const& t_) noexcept -> RawFrame {
			RawFrame f{ frame_ };
			auto const stick{
				[&f](Analog ax_, Analog ay_, StickZone z_) {
					float x{ f.AnalogValue(ax_) };
					float y{ f.AnalogValue(ay_) };
					ShapeStick(x, y, z_);
					f.SetAnalog(ax_, x);
					f.SetAnalog(ay_, y);
				}
			};
			stick(Analog::LeftStickX, Analog::LeftStickY, t_.LeftStick);
			stick(Analog::RightStickX, Analog::RightStickY, t_.RightStick);
			f.SetAnalog(Analog::LeftTrigger, ShapeTrigger(f.AnalogValue(Analog::LeftTrigger), t_.TriggerInner));
			f.SetAnalog(Analog::RightTrigger, ShapeTrigger(f.AnalogValue(Analog::RightTrigger), t_.TriggerInner));
			return f;
		}

		auto HasKeyboardMouseInput(RawFrame const& f_) noexcept -> bool {
			return f_.Keys.any() || f_.MouseButtons.any() ||
				f_.AnalogValue(Analog::MouseX) != 0.0f || f_.AnalogValue(Analog::MouseY) != 0.0f || f_.AnalogValue(Analog::Wheel) != 0.0f;
		}

		// shaped_ はデッドゾーンを掛けた後のフレーム
		auto HasGamepadInput(RawFrame const& shaped_) noexcept -> bool {
			if (shaped_.PadButtons.any()) { return true; }
			constexpr std::array<Analog, 6> padAxes{
				Analog::LeftStickX, Analog::LeftStickY, Analog::RightStickX, Analog::RightStickY, Analog::LeftTrigger, Analog::RightTrigger
			};
			for (Analog const a : padAxes) {
				if (shaped_.AnalogValue(a) != 0.0f) { return true; }
			}
			return false;
		}

		const ActionState EmptyState{};
	}

	auto IsValid(GamepadTuning const& t_) noexcept -> bool {
		return IsValid(t_.LeftStick) && IsValid(t_.RightStick) &&
			std::isfinite(t_.TriggerInner) && t_.TriggerInner >= 0.0f && t_.TriggerInner < 1.0f;
	}

	void ShapeStick(float& x_, float& y_, StickZone zone_) noexcept {
		if (!std::isfinite(x_) || !std::isfinite(y_)) { x_ = 0.0f; y_ = 0.0f; return; }
		float const len{ std::sqrt(x_ * x_ + y_ * y_) };
		if (!(len > zone_.Inner) || !(zone_.Outer > zone_.Inner)) { x_ = 0.0f; y_ = 0.0f; return; }
		// Inner..Outer → 0..1。向きはそのまま（len > Inner >= 0 なので割り算は安全）
		float const t{ std::clamp((len - zone_.Inner) / (zone_.Outer - zone_.Inner), 0.0f, 1.0f) };
		float const k{ t / len };
		x_ *= k;
		y_ *= k;
	}

	auto ShapeTrigger(float v_, float inner_) noexcept -> float {
		if (!std::isfinite(v_) || !(v_ > inner_) || !(inner_ < 1.0f)) { return 0.0f; }
		return std::clamp((v_ - inner_) / (1.0f - inner_), 0.0f, 1.0f);
	}

	auto ReadSource(RawFrame const& frame_, Source src_) noexcept -> float {
		if (!IsValidSource(src_)) { return 0.0f; }
		switch (src_.Kind) {
		case SourceKind::Key:			return frame_.Keys.test(src_.Code) ? 1.0f : 0.0f;
		case SourceKind::MouseButton:	return frame_.MouseButtons.test(src_.Code) ? 1.0f : 0.0f;
		case SourceKind::GamepadButton:	return frame_.PadButtons.test(src_.Code) ? 1.0f : 0.0f;
		case SourceKind::Analog:		return frame_.Analogs[src_.Code];
		default:						return 0.0f;
		}
	}

	//====	======	======	======	======	====//
	//	InputMap								//
	//====	======	======	======	======	====//

	auto InputMap::Define(ActionId id_, ActionKind kind_, bool clampToUnit_) -> bool {
		if (id_ >= MaxActions || kind_ == ActionKind::Undefined) { return false; }
		Descs_[id_] = ActionDesc{ kind_, clampToUnit_ };
		Unbind(id_);
		return true;
	}

	auto InputMap::Bind(ActionId id_, Source src_, Component comp_, float scale_) -> bool {
		if (id_ >= MaxActions) { return false; }
		ActionKind const kind{ Descs_[id_].Kind };
		if (kind == ActionKind::Undefined) { return false; }
		if (!IsValidSource(src_)) { return false; }
		if (comp_ == Component::Y && kind != ActionKind::Axis2D) { return false; }
		if (!std::isfinite(scale_)) { return false; }
		Bindings_.push_back(Binding{ id_, src_, comp_, scale_ });
		return true;
	}

	void InputMap::Unbind(ActionId id_) {
		std::erase_if(Bindings_, [id_](Binding const& b_) { return b_.Action == id_; });
	}

	auto InputMap::Desc(ActionId id_) const noexcept -> ActionDesc {
		return (id_ < MaxActions) ? Descs_[id_] : ActionDesc{};
	}

	auto InputMap::SetGamepadTuning(GamepadTuning const& t_) noexcept -> bool {
		if (!IsValid(t_)) { return false; }
		Gamepad_ = t_;
		return true;
	}

	//====	======	======	======	======	====//
	//	InputState								//
	//====	======	======	======	======	====//

	void InputState::Update(InputMap const& map_, RawFrame const& frame_) { Evaluate(map_, frame_, true); }
	void InputState::Sync(InputMap const& map_, RawFrame const& frame_) { Evaluate(map_, frame_, false); }

	void InputState::Clear() noexcept { States_.fill(ActionState{}); }	// LastDevice は残す（表示が一瞬切り替わらないように）

	auto InputState::State(ActionId id_) const noexcept -> ActionState const& {
		return (id_ < MaxActions) ? States_[id_] : EmptyState;
	}

	auto InputState::PressedMask() const noexcept -> ActionMask {
		ActionMask m{ 0U };
		for (std::size_t i = 0; i < MaxActions; ++i) {
			if (States_[i].Pressed) { m |= ActionMask{ 1U } << i; }
		}
		return m;
	}

	void InputState::Evaluate(InputMap const& map_, RawFrame const& frame_, bool edges_) {
		RawFrame const shaped{ ShapePad(frame_, map_.Gamepad()) };

		bool const kbm{ HasKeyboardMouseInput(shaped) };
		bool const pad{ HasGamepadInput(shaped) };
		if (kbm != pad) { LastDevice_ = kbm ? Device::KeyboardMouse : Device::Gamepad; }

		std::array<float, MaxActions> sumX{};
		std::array<float, MaxActions> sumY{};
		std::array<bool, MaxActions> buttonDown{};

		for (Binding const& b : map_.Bindings()) {
			float const v{ ReadSource(shaped, b.Src) * b.Scale };
			if (map_.Desc(b.Action).Kind == ActionKind::Button) {
				// Button は足し合わせない（Space と Enter を両方押しても 2 にならない、どれか 1 つで押している）
				if (v >= ButtonThreshold) { buttonDown[b.Action] = true; }
			}
			else if (b.Comp == Component::X) { sumX[b.Action] += v; }
			else { sumY[b.Action] += v; }
		}

		for (std::size_t i = 0; i < MaxActions; ++i) {
			ActionDesc const desc{ map_.Desc(static_cast<ActionId>(i)) };
			ActionState& s{ States_[i] };

			float x{ 0.0f };
			float y{ 0.0f };
			bool down{ false };
			switch (desc.Kind) {
			case ActionKind::Button:
				down = buttonDown[i];
				x = down ? 1.0f : 0.0f;
				break;
			case ActionKind::Axis1D:
				x = sumX[i];
				if (desc.ClampToUnit) { x = std::clamp(x, -1.0f, 1.0f); }
				down = (x != 0.0f);
				break;
			case ActionKind::Axis2D: {
				x = sumX[i];
				y = sumY[i];
				if (desc.ClampToUnit) {
					float const lenSq{ x * x + y * y };
					if (lenSq > 1.0f) {
						float const inv{ 1.0f / std::sqrt(lenSq) };
						x *= inv;
						y *= inv;
					}
				}
				down = (x != 0.0f || y != 0.0f);
				break;
			}
			default:
				break;
			}

			bool const wasDown{ s.Down };
			std::uint32_t const prevHeld{ s.HeldFrames };

			s.X = x;
			s.Y = y;
			s.Down = down;
			s.Pressed = edges_ && down && !wasDown;
			s.Released = edges_ && !down && wasDown;
			s.ReleasedHeldFrames = s.Released ? prevHeld : 0U;
			if (!down) { s.HeldFrames = 0U; }
			else if (wasDown) { s.HeldFrames = prevHeld + 1U; }
			else { s.HeldFrames = edges_ ? 1U : 0U; }	// Sync で押しっぱなし扱いにしたものは 0 から数える（いつ押したか分からないので）
		}
	}
}
