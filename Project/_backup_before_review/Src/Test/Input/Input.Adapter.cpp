module;

#define NOMINMAX
#include <Windows.h>
#include <Xinput.h>

// XInput 1.4（Windows 8 以降に入っている）。vcxproj の追加の依存ファイルを変えずに済むようにここでリンクする
#pragma comment(lib, "Xinput.lib")

module Lumina.Input.Adapter;

//****	******	******	******	******	****//

import <cstdint>;

import Lumina.Input;
import Lumina.WinApp.RawInput;

//////	//////	//////	//////	//////	//////

namespace Lumina::Input::Adapter {
	namespace {
		// スティックは -32768..32767。-32768 は -1 に収める（左右で範囲が 1 だけ違うため）
		auto NormalizeStick(SHORT v_) noexcept -> float {
			float const f{ static_cast<float>(v_) / 32767.0f };
			return (f < -1.0f) ? -1.0f : f;
		}

		// トリガーは 0..255
		auto NormalizeTrigger(BYTE v_) noexcept -> float {
			return static_cast<float>(v_) / 255.0f;
		}

		struct ButtonMap {
			WORD Mask;
			GamepadButton Button;
		};

		constexpr ButtonMap PadButtonMap[]{
			{ XINPUT_GAMEPAD_A, GamepadButton::A },
			{ XINPUT_GAMEPAD_B, GamepadButton::B },
			{ XINPUT_GAMEPAD_X, GamepadButton::X },
			{ XINPUT_GAMEPAD_Y, GamepadButton::Y },
			{ XINPUT_GAMEPAD_LEFT_SHOULDER, GamepadButton::LeftShoulder },
			{ XINPUT_GAMEPAD_RIGHT_SHOULDER, GamepadButton::RightShoulder },
			{ XINPUT_GAMEPAD_BACK, GamepadButton::Back },
			{ XINPUT_GAMEPAD_START, GamepadButton::Start },
			{ XINPUT_GAMEPAD_LEFT_THUMB, GamepadButton::LeftThumb },
			{ XINPUT_GAMEPAD_RIGHT_THUMB, GamepadButton::RightThumb },
			{ XINPUT_GAMEPAD_DPAD_UP, GamepadButton::DpadUp },
			{ XINPUT_GAMEPAD_DPAD_DOWN, GamepadButton::DpadDown },
			{ XINPUT_GAMEPAD_DPAD_LEFT, GamepadButton::DpadLeft },
			{ XINPUT_GAMEPAD_DPAD_RIGHT, GamepadButton::DpadRight },
		};

		void ClearPad(RawFrame& out_) noexcept {
			out_.PadButtons.reset();
			out_.SetAnalog(Analog::LeftStickX, 0.0f);
			out_.SetAnalog(Analog::LeftStickY, 0.0f);
			out_.SetAnalog(Analog::RightStickX, 0.0f);
			out_.SetAnalog(Analog::RightStickY, 0.0f);
			out_.SetAnalog(Analog::LeftTrigger, 0.0f);
			out_.SetAnalog(Analog::RightTrigger, 0.0f);
		}

		void FillPad(XINPUT_GAMEPAD const& pad_, RawFrame& out_) noexcept {
			for (ButtonMap const& m : PadButtonMap) {
				out_.SetPad(m.Button, (pad_.wButtons & m.Mask) != 0);
			}
			out_.SetAnalog(Analog::LeftStickX, NormalizeStick(pad_.sThumbLX));
			out_.SetAnalog(Analog::LeftStickY, NormalizeStick(pad_.sThumbLY));
			out_.SetAnalog(Analog::RightStickX, NormalizeStick(pad_.sThumbRX));
			out_.SetAnalog(Analog::RightStickY, NormalizeStick(pad_.sThumbRY));
			out_.SetAnalog(Analog::LeftTrigger, NormalizeTrigger(pad_.bLeftTrigger));
			out_.SetAnalog(Analog::RightTrigger, NormalizeTrigger(pad_.bRightTrigger));
		}
	}

	//====	======	======	======	======	====//
	//	RawInputSampler							//
	//====	======	======	======	======	====//

	void RawInputSampler::Capture(WinApp::RawInput const& input_, RawFrame& out_, bool blockKeys_, bool blockMouse_) {
		using WinApp::KEY;
		out_ = RawFrame{};

		//----	------	------	------	------	----//
		// キー

		if (!blockKeys_) {
			auto const& keyboard{ input_.Keyboard() };
			for (std::uint32_t code = 0; code < KeyCount; ++code) {
				out_.SetKey(code, keyboard.IsPressed(static_cast<KEY>(code)));
			}
			// RawInput は Shift / Ctrl / Alt を左右に分けて記録する（左右を区別しないコードは立たない）ので、ここで立てる
			auto const either{
				[&](KEY any_, KEY left_, KEY right_) {
					bool const down{ keyboard.IsPressed(left_) || keyboard.IsPressed(right_) };
					out_.SetKey(static_cast<std::uint32_t>(any_), down);
				}
			};
			either(KEY::SHIFT, KEY::SHIFT_LEFT, KEY::SHIFT_RIGHT);
			either(KEY::CTRL, KEY::CTRL_LEFT, KEY::CTRL_RIGHT);
			either(KEY::ALT, KEY::ALT_LEFT, KEY::ALT_RIGHT);
		}

		//----	------	------	------	------	----//
		// マウス

		auto const& mouse{ input_.Mouse() };
		std::int64_t const x{ mouse.TotalDeltaX() };
		std::int64_t const y{ mouse.TotalDeltaY() };
		std::int32_t const wheel{ mouse.Wheel() };

		if (!blockMouse_) {
			out_.SetMouse(MouseButton::Left, mouse.LeftButton() != 0);
			out_.SetMouse(MouseButton::Right, mouse.RightButton() != 0);
			if (HasPrev_) {
				out_.SetAnalog(Analog::MouseX, static_cast<float>(x - PrevX_));
				out_.SetAnalog(Analog::MouseY, static_cast<float>(y - PrevY_));
				out_.SetAnalog(Analog::Wheel, static_cast<float>(wheel - PrevWheel_));
			}
		}

		HasPrev_ = true;
		PrevX_ = x;
		PrevY_ = y;
		PrevWheel_ = wheel;
	}

	//====	======	======	======	======	====//
	//	XInputSampler							//
	//====	======	======	======	======	====//

	void XInputSampler::Capture(RawFrame& out_, bool block_) {
		ClearPad(out_);

		XINPUT_STATE state{};
		if (Connected_) {
			if (::XInputGetState(static_cast<DWORD>(UserIndex_), &state) != ERROR_SUCCESS) {
				// 抜けた。次に探すのは RetryFrames_ 後
				Connected_ = false;
				Wait_ = RetryFrames_;
				return;
			}
		}
		else {
			// つながっていないスロットを毎フレーム問い合わせると重いので、間隔を空ける
			if (Wait_ > 0U) {
				--Wait_;
				return;
			}
			bool found{ false };
			for (DWORD i = 0; i < static_cast<DWORD>(XUSER_MAX_COUNT); ++i) {
				if (::XInputGetState(i, &state) == ERROR_SUCCESS) {
					UserIndex_ = static_cast<std::uint32_t>(i);
					found = true;
					break;
				}
			}
			if (!found) {
				Wait_ = RetryFrames_;
				return;
			}
			Connected_ = true;
		}

		if (block_) { return; }
		FillPad(state.Gamepad, out_);
	}

	//====	======	======	======	======	====//
	//	CursorLock								//
	//====	======	======	======	======	====//

	void CursorLock::Update(bool want_) {
		HWND const hwnd{ ::GetActiveWindow() };		// このスレッドのウィンドウが前面でなければ nullptr
		bool const focused{ hwnd != nullptr && ::GetForegroundWindow() == hwnd };
		if (!want_ || !focused) {
			Release();
			return;
		}

		// クライアント領域（スクリーン座標）。ウィンドウが動くこともあるので毎フレーム取り直す
		RECT client{};
		::GetClientRect(hwnd, &client);
		POINT topLeft{ client.left, client.top };
		POINT bottomRight{ client.right, client.bottom };
		::ClientToScreen(hwnd, &topLeft);
		::ClientToScreen(hwnd, &bottomRight);
		RECT const clip{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
		::ClipCursor(&clip);

		if (!Locked_) {
			// 掛けた瞬間だけ、カーソルを中央へ（隠れたカーソルがウィンドウの端に残らないように）
			::SetCursorPos((topLeft.x + bottomRight.x) / 2, (topLeft.y + bottomRight.y) / 2);
			::ShowCursor(FALSE);
			Locked_ = true;
		}
	}

	void CursorLock::Release() noexcept {
		if (!Locked_) { return; }
		::ClipCursor(nullptr);
		::ShowCursor(TRUE);
		Locked_ = false;
	}
}
