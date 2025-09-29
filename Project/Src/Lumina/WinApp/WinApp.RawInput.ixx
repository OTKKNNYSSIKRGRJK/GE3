module;

#include<Windows.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.WinApp.RawInput;

//****	******	******	******	******	****//

import <cstdint>;

import <memory>;
import <functional>;

import <unordered_map>;

import <string>;

import Lumina.Mixins;
import Lumina.Container.Bitset;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace Lumina::WinApp {
	class RawKeyboard;
	class RawMouse;

	class RawInput;
}

//****	******	******	******	******	****//

namespace Lumina::WinApp {
	enum class RAW_INPUT_DEVICE_USAGE_PAGE : uint16_t {
		GENERIC_DESKTOP_PAGE = 0x01,
	};

	enum class RAW_INPUT_DEVICE_USAGE : uint16_t {
		MOUSE = 0x02,
		JOYSTICK = 0x04,
		GAMEPAD = 0x05,
		KEYBOARD = 0x06,
	};

	// Based on Windows virtual-key codes
	// https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	export enum class KEY {
		A = 0x41, B = 0x42, C = 0x43, D = 0x44,
		E = 0x45, F = 0x46, G = 0x47, H = 0x48,
		I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C,
		M = 0x4D, N = 0x4E, O = 0x4F, P = 0x50,
		Q = 0x51, R = 0x52, S = 0x53, T = 0x54,
		U = 0x55, V = 0x56, W = 0x57, X = 0x58,
		Y = 0x59, Z = 0x5A,

		NUM_0 = 0x30, NUM_1 = 0x31,
		NUM_2 = 0x32, NUM_3 = 0x33,
		NUM_4 = 0x34, NUM_5 = 0x35,
		NUM_6 = 0x36, NUM_7 = 0x37,
		NUM_8 = 0x38, NUM_9 = 0x39,

		NUMPAD_0 = 0x60, NUMPAD_1 = 0x61,
		NUMPAD_2 = 0x62, NUMPAD_3 = 0x63,
		NUMPAD_4 = 0x64, NUMPAD_5 = 0x65,
		NUMPAD_6 = 0x66, NUMPAD_7 = 0x67,
		NUMPAD_8 = 0x68, NUMPAD_9 = 0x69,

		F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73,
		F5 = 0x74, F6 = 0x75, F7 = 0x76, F8 = 0x77,
		F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,

		ARROW_LEFT = VK_LEFT,
		ARROW_RIGHT = VK_RIGHT,
		ARROW_UP = VK_UP,
		ARROW_DOWN = VK_DOWN,

		ENTER = VK_RETURN,
		BACKSPACE = VK_BACK,
		SPACE = VK_SPACE,
		ESC = VK_ESCAPE,

		SHIFT = VK_SHIFT,
		SHIFT_LEFT = VK_LSHIFT,
		SHIFT_RIGHT = VK_RSHIFT,
		CTRL = VK_CONTROL,
		CTRL_LEFT = VK_LCONTROL,
		CTRL_RIGHT = VK_RCONTROL,
		ALT = VK_MENU,
		ALT_LEFT = VK_LMENU,
		ALT_RIGHT = VK_RMENU,

		HOME = VK_HOME,
		END = VK_END,
		PAGE_UP = VK_PRIOR,
		PAGE_DOWN = VK_DOWN,
	};
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	RawKeyboard								//
//////	//////	//////	//////	//////	//////

namespace Lumina::WinApp {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	class RawKeyboard final :
		public NonCopyable<RawKeyboard> {
		friend RawInput;

		//====	======	======	======	======	====//

	public:
		constexpr bool IsPressed(KEY key_) const noexcept;
		constexpr bool IsReleased(KEY key_) const noexcept;

		inline void CurrentState(Bitset<256U>& state_) const {
			state_.Set(CurrentState_);
		}

		//----	------	------	------	------	----//

	private:
		constexpr void OnInput(RAWINPUT const& rawInput_) noexcept;

		//----	------	------	------	------	----//

	private:
		void Initialize(HWND hWnd_);
		void Finalize();

		//----	------	------	------	------	----//

	private:
		RawKeyboard() noexcept;
	public:
		~RawKeyboard() noexcept;

		//====	======	======	======	======	====//

	private:
		Bitset<256U> CurrentState_{};
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	constexpr bool RawKeyboard::IsPressed(KEY key_) const noexcept {
		auto keyCode{ static_cast<uint32_t>(key_) };
		return CurrentState_[keyCode];
	}
	constexpr bool RawKeyboard::IsReleased(KEY key_) const noexcept {
		auto keyCode{ static_cast<uint32_t>(key_) };
		return !CurrentState_[keyCode];
	}

	//----	------	------	------	------	----//

	constexpr void RawKeyboard::OnInput(RAWINPUT const& rawInput_) noexcept {
		if (rawInput_.header.dwType != RIM_TYPEKEYBOARD) { return; }

		RAWKEYBOARD const& keyboard{ rawInput_.data.keyboard };

		bool isKeyPressed{ (keyboard.Flags & 0x01U) == RI_KEY_MAKE };
		KEY key{ static_cast<KEY>(keyboard.VKey) };

		if (key == KEY::SHIFT) {
			key = (keyboard.MakeCode == 0x36U) ? (KEY::SHIFT_RIGHT) : (KEY::SHIFT_LEFT);
			/*if (!isKeyPressed) {
				CurrentState_.Set(static_cast<uint32_t>(KEY::SHIFT_LEFT), isKeyPressed);
				CurrentState_.Set(static_cast<uint32_t>(KEY::SHIFT_RIGHT), isKeyPressed);
			}*/
		}
		if (key == KEY::CTRL) {
			key = ((keyboard.Flags & 0x02U) == RI_KEY_E0) ? (KEY::CTRL_RIGHT) : (KEY::CTRL_LEFT);
		}
		if (key == KEY::ALT) {
			key = ((keyboard.Flags & 0x02U) == RI_KEY_E0) ? (KEY::ALT_RIGHT) : (KEY::ALT_LEFT);
		}

		CurrentState_.Set(static_cast<uint32_t>(key), isKeyPressed);
	}

	//----	------	------	------	------	----//

	void RawKeyboard::Initialize(HWND hWnd_) {
		RAWINPUTDEVICE rid{
			.usUsagePage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE_PAGE::GENERIC_DESKTOP_PAGE) },
			.usUsage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE::KEYBOARD) },
			.dwFlags{ 0U },
			.hwndTarget{ hWnd_ },
		};
		::RegisterRawInputDevices(&rid, 1U, sizeof(RAWINPUTDEVICE)) ||
		Utils::Debug::ThrowIfFalse{ "<RawKeyboard> Register failed!\n" };
	}

	void RawKeyboard::Finalize() {
		RAWINPUTDEVICE rid{
			.usUsagePage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE_PAGE::GENERIC_DESKTOP_PAGE) },
			.usUsage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE::KEYBOARD) },
			.dwFlags{ RIDEV_REMOVE },
			.hwndTarget{ nullptr },
		};
		::RegisterRawInputDevices(&rid, 1U, sizeof(RAWINPUTDEVICE)) ||
		Utils::Debug::ThrowIfFalse{ "<RawKeyboard> Unregister failed!\n" };
	}

	//----	------	------	------	------	----//

	RawKeyboard::RawKeyboard() noexcept {}

	RawKeyboard::~RawKeyboard() noexcept {}
}

//////	//////	//////	//////	//////	//////
//	RawMouse								//
//////	//////	//////	//////	//////	//////

namespace Lumina::WinApp {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	class RawMouse final :
		public NonCopyable<RawMouse> {
		friend RawInput;

		//====	======	======	======	======	====//

	private:
		struct State {
			int32_t LeftButton;
			int32_t RightButton;
			int32_t Wheel;
		};

		//====	======	======	======	======	====//

	public:
		constexpr int32_t PosX() const noexcept;
		constexpr int32_t PosY() const noexcept;
		constexpr int32_t DeltaX() const noexcept;
		constexpr int32_t DeltaY() const noexcept;

		constexpr int32_t LeftButton() const noexcept;
		constexpr int32_t RightButton() const noexcept;
		constexpr int32_t Wheel() const noexcept;
		constexpr int32_t DeltaWheel() const noexcept;

	private:
		inline void OnInput(RAWINPUT const& rawInput_) noexcept;
		constexpr void OnMove(LPARAM lParam_) noexcept;

		//----	------	------	------	------	----//

	private:
		void Initialize(HWND hWnd_);
		void Finalize();

		//----	------	------	------	------	----//

	private:
		RawMouse() noexcept;
	public:
		~RawMouse() noexcept;

		//====	======	======	======	======	====//

	private:
		int32_t PosX_{ 0 };
		int32_t PosY_{ 0 };
		int32_t DeltaX_{ 0 };
		int32_t DeltaY_{ 0 };

		RawMouse::State CurrentState_{ .LeftButton{ 0 }, .RightButton{ 0 }, .Wheel{ 0 } };
		RawMouse::State PreviousState_{ .LeftButton{ 0 }, .RightButton{ 0 }, .Wheel{ 0 } };
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	constexpr int32_t RawMouse::PosX() const noexcept { return PosX_; }
	constexpr int32_t RawMouse::PosY() const noexcept { return PosY_; }
	constexpr int32_t RawMouse::DeltaX() const noexcept { return DeltaX_; }
	constexpr int32_t RawMouse::DeltaY() const noexcept { return DeltaY_; }

	constexpr int32_t RawMouse::LeftButton() const noexcept { return CurrentState_.LeftButton; }
	constexpr int32_t RawMouse::RightButton() const noexcept { return CurrentState_.RightButton; }
	constexpr int32_t RawMouse::Wheel() const noexcept { return CurrentState_.Wheel; }
	constexpr int32_t RawMouse::DeltaWheel() const noexcept { return CurrentState_.Wheel - PreviousState_.Wheel; }

	//----	------	------	------	------	----//

	constexpr void RawMouse::OnMove(LPARAM lParam_) noexcept {
		PosX_ = static_cast<int16_t>(LOWORD(lParam_));
		PosY_ = static_cast<int16_t>(HIWORD(lParam_));
	}

	inline void RawMouse::OnInput(RAWINPUT const& rawInput_) noexcept {
		if (rawInput_.header.dwType != RIM_TYPEMOUSE) { return; }

		std::memcpy(&PreviousState_, &CurrentState_, sizeof(RawMouse::State));

		RAWMOUSE const& mouse{ rawInput_.data.mouse };
		if ((mouse.usFlags & 0x01U) == MOUSE_MOVE_RELATIVE) {
			DeltaX_ = mouse.lLastX;
			DeltaY_ = mouse.lLastY;
		}
		if (mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_DOWN) { CurrentState_.LeftButton = 1; }
		else if (mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_UP) { CurrentState_.LeftButton = 0; }
		if (mouse.ulButtons & RI_MOUSE_RIGHT_BUTTON_DOWN) { CurrentState_.RightButton = 1; }
		else if (mouse.ulButtons & RI_MOUSE_RIGHT_BUTTON_UP) { CurrentState_.RightButton = 0; }
		if (mouse.ulButtons & RI_MOUSE_WHEEL) {
			CurrentState_.Wheel += static_cast<int16_t>(mouse.usButtonData) / WHEEL_DELTA;
		}
	}

	void RawMouse::Initialize(HWND hWnd_) {
		RAWINPUTDEVICE rid{
			.usUsagePage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE_PAGE::GENERIC_DESKTOP_PAGE) },
			.usUsage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE::MOUSE) },
			.dwFlags{ RIDEV_INPUTSINK },
			.hwndTarget{ hWnd_ },
		};
		::RegisterRawInputDevices(&rid, 1U, sizeof(RAWINPUTDEVICE)) ||
		Utils::Debug::ThrowIfFalse{ "<RawMouse> Register failed!\n" };
	}

	void RawMouse::Finalize() {
		RAWINPUTDEVICE rid{
			.usUsagePage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE_PAGE::GENERIC_DESKTOP_PAGE) },
			.usUsage{ static_cast<uint16_t>(RAW_INPUT_DEVICE_USAGE::MOUSE) },
			.dwFlags{ RIDEV_REMOVE },
			.hwndTarget{ nullptr },
		};
		::RegisterRawInputDevices(&rid, 1U, sizeof(RAWINPUTDEVICE)) ||
		Utils::Debug::ThrowIfFalse{ "<RawMouse> Register failed!\n" };
	}

	//----	------	------	------	------	----//

	RawMouse::RawMouse() noexcept {}

	RawMouse::~RawMouse() noexcept {}
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	RawInput								//
//////	//////	//////	//////	//////	//////

namespace Lumina::WinApp {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class RawInput final :
		public NonCopyable<RawInput> {
		using Message = UINT;
		using MessageProcessor = void(RawInput::*)(WPARAM, LPARAM);

		//====	======	======	======	======	====//

	public:
		constexpr auto const& Keyboard() const noexcept;
		constexpr auto const& Mouse() const noexcept;

		//----	------	------	------	------	----//

	public:
		auto const& Callback() const noexcept;

		//----	------	------	------	------	----//

	private:
		void OnInput(WPARAM wParam_, LPARAM lParam_);
		void OnMouseMove(WPARAM wParam_, LPARAM lParam_);

		LRESULT CALLBACK WindowProcedure(
			HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_
		);

		//----	------	------	------	------	----//

	public:
		void Initialize(HWND hWnd_);
		void Finalize() noexcept;

		//----	------	------	------	------	----//

	public:
		RawInput() noexcept;
		~RawInput() noexcept;

		//====	======	======	======	======	====//

	private:
		std::function<LRESULT CALLBACK(HWND, UINT, WPARAM, LPARAM)> Callback_{ nullptr };

		std::unordered_map<Message, MessageProcessor> MessageProcessors_{};

		RawKeyboard* Keyboard_{ nullptr };
		RawMouse* Mouse_{ nullptr };

		__declspec(align(4U)) byte* Buffer_{};
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	constexpr auto const& RawInput::Keyboard() const noexcept { return *Keyboard_; }
	constexpr auto const& RawInput::Mouse() const noexcept { return *Mouse_; }

	//----	------	------	------	------	----//

	auto const& RawInput::Callback() const noexcept {
		return Callback_;
	}

	//----	------	------	------	------	----//

	void RawInput::OnInput([[maybe_unused]] WPARAM wParam_, [[maybe_unused]] LPARAM lParam_) {
		auto hndl{ reinterpret_cast<HRAWINPUT>(lParam_) };
		uint32_t dataSize{};
		// Obtains the size of the raw input data.
		::GetRawInputData(hndl, RID_INPUT, nullptr, &dataSize, sizeof(RAWINPUTHEADER));
		// Obtains the raw input data.
		::GetRawInputData(hndl, RID_INPUT, Buffer_, &dataSize, sizeof(RAWINPUTHEADER));

		auto const& rawInputData{ *reinterpret_cast<RAWINPUT const*>(Buffer_) };
		Keyboard_->OnInput(rawInputData);
		Mouse_->OnInput(rawInputData);
	}
	
	void RawInput::OnMouseMove([[maybe_unused]] WPARAM wParam_, [[maybe_unused]] LPARAM lParam_) {
		Mouse_->OnMove(lParam_);
	}

	LRESULT CALLBACK RawInput::WindowProcedure(
		[[maybe_unused]] HWND hWnd_, UINT msg_, [[maybe_unused]] WPARAM wParam_, LPARAM lParam_
	) {
		auto&& it{ MessageProcessors_.find(msg_) };
		if (it != MessageProcessors_.cend()) { (this->*(it->second))(wParam_, lParam_); }

		//switch (msg_) {
		//	case WM_ACTIVATEAPP:
		//		return MA_ACTIVATEANDEAT;

		//	case WM_INPUT:
		//	case WM_MOUSEMOVE:
		//	case WM_LBUTTONDOWN:
		//	case WM_LBUTTONUP:
		//	case WM_RBUTTONDOWN:
		//	case WM_RBUTTONUP:
		//	case WM_MBUTTONDOWN:
		//	case WM_MBUTTONUP:
		//	case WM_MOUSEWHEEL:
		//	case WM_XBUTTONDOWN:
		//	case WM_XBUTTONUP:
		//	case WM_MOUSEHOVER:
		//		//DirectX::Mouse::ProcessMessage(msg_, wParam_, lParam_);
		//		break;
		//}

		return 0;
	}

	//----	------	------	------	------	----//

	void RawInput::Initialize(HWND hWnd_) {
		if (Keyboard_ == nullptr) {
			Keyboard_ = new RawKeyboard{};
			Keyboard_->Initialize(hWnd_);
		}
		if (Mouse_ == nullptr) {
			Mouse_ = new RawMouse{};
			Mouse_->Initialize(hWnd_);
		}

		if (Buffer_ == nullptr) {
			Buffer_ = new byte[256U];
		}

		Callback_ =
			std::bind(
				&RawInput::WindowProcedure,
				this,
				std::placeholders::_1,
				std::placeholders::_2,
				std::placeholders::_3,
				std::placeholders::_4
			);

		MessageProcessors_.emplace(WM_INPUT, &RawInput::OnInput);
		MessageProcessors_.emplace(WM_MOUSEMOVE, &RawInput::OnMouseMove);
	}

	void RawInput::Finalize() noexcept {
		if (Keyboard_ != nullptr) {
			Keyboard_->Finalize();
			delete Keyboard_;
			Keyboard_ = nullptr;
		}
		if (Mouse_ != nullptr) {
			Mouse_->Finalize();
			delete Mouse_;
			Mouse_ = nullptr;
		}

		if (Buffer_ != nullptr) {
			delete Buffer_;
			Buffer_ = nullptr;
		}
	}

	//----	------	------	------	------	----//

	RawInput::RawInput() noexcept {}

	RawInput::~RawInput() noexcept {
		Finalize();
	}
}