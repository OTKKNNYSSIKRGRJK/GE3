module;

#include<Windows.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.WinApp;

import <cstdint>;

import <functional>;

import <string>;
import <format>;

import <cassert>;

import Lumina.Utils.String;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace {
	template<typename CharType, uint32_t Size>
	using LiteralString = Lumina::Utils::String::Literal<CharType, Size>;
}

namespace Lumina::WinApp {
	export using CallbackFunctor = std::function<LRESULT CALLBACK(HWND, UINT, WPARAM, LPARAM)>;
}

//////	//////	//////	//////	//////	//////

export namespace Lumina::WinApp {
	template<LiteralString Tag>
	class WindowClass {
	private:
		class WindowProcedure {
			friend WindowClass;

		public:
			static LRESULT CALLBACK Callback(HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_) {
				for (auto const& callback : Subcallbacks_) {
					if (callback(hWnd_, msg_, wParam_, lParam_)) { return 1; }
				}

				if (msg_ == WM_DESTROY) {
					::PostQuitMessage(0);
					return 0;
				}

				return ::DefWindowProc(hWnd_, msg_, wParam_, lParam_);
			}

		private:
			static inline std::vector<CallbackFunctor> Subcallbacks_{};
		};

		//====	======	======	======	======	====//

	public:
		constexpr std::wstring_view Name() const noexcept { return Tag(); }

	public:
		void RegisterCallback(CallbackFunctor const& callback_);
		void ClearCallbacks();

		//----	------	------	------	------	----//

	public:
		void Initialize(HCURSOR cursor_ = ::LoadCursor(nullptr, IDC_ARROW));

		//----	------	------	------	------	----//

	public:
		constexpr WindowClass() noexcept;
		constexpr ~WindowClass() noexcept;

		//====	======	======	======	======	====//

	protected:
		WNDCLASSEX Properties_{};
	};

	class Window {
	public:
		struct Config {
			std::wstring Name;

			std::wstring Title;
			uint32_t Style;
			uint32_t ClientWidth;
			uint32_t ClientHeight;
		};

		//====	======	======	======	======	====//

	public:
		enum class SIZEMODE : uint32_t {
			FixedSize = 0U,
			FixedAspectRatio = 1U,
			Fullscreen = 2U,
		};

		enum class STYLE : uint32_t {
			TitleBar = WS_CAPTION,
			WindowMenu = WS_SYSMENU,
			MinimizeButton = WS_MINIMIZEBOX,
		};

		//====	======	======	======	======	====//

	public:
		constexpr HWND Handle() const noexcept { return Handle_; }
		constexpr std::wstring_view Name() const noexcept { return Config_.Name; }
		constexpr std::wstring_view Title() const noexcept { return Config_.Title; }
		constexpr uint32_t Style() const noexcept { return Config_.Style; }
		constexpr uint32_t ClientWidth() const noexcept { return Config_.ClientWidth; }
		constexpr uint32_t ClientHeight() const noexcept { return Config_.ClientHeight; }

		//----	------	------	------	------	----//

		constexpr void SizeMode(SIZEMODE sizeMode_, uint32_t val_) const noexcept {
			Bitset_SizeMode_ |= (1U << static_cast<uint32_t>(sizeMode_));
			Bitset_SizeMode_ &= ((1U & !!(val_)) << static_cast<uint32_t>(sizeMode_));
			// Fixed size implies fixed aspect ratio.
			if (Bitset_SizeMode_ & 0b001U) { Bitset_SizeMode_ |= 0b010U; }
		}
		constexpr bool SizeMode(SIZEMODE sizeMode_) const noexcept {
			return !!(Bitset_SizeMode_ & (1U << static_cast<uint32_t>(sizeMode_)));
		}

		//----	------	------	------	------	----//

		// Adjust the window size.
		void Resize(WPARAM evtParam_Sizing_, LPARAM evtParam_Rect_) const;

		//----	------	------	------	------	----//

		void Initialize(
			std::wstring_view className_,
			Config const& config_
		);

		//----	------	------	------	------	----//

	public:
		constexpr Window() noexcept;
		constexpr ~Window() noexcept;

		//====	======	======	======	======	====//

	protected:
		HWND Handle_{ nullptr };				// Window handle
		std::wstring_view ClassName_{};			// Window class name
		Config mutable Config_{};

		float mutable AspectRatio_{ 1.0f };
		// Default SizeMode:
		// - FixedSize: yes
		// - FixedAspectRatio: yes (because size is fixed)
		// - Fullscreen: no
		uint32_t mutable Bitset_SizeMode_{ 0b011U };
	};
}

namespace Lumina::WinApp {
	template<LiteralString Tag>
	void WindowClass<Tag>::RegisterCallback(CallbackFunctor const& callback_) {
		WindowProcedure::Subcallbacks_.emplace_back(callback_);
	}

	template<LiteralString Tag>
	void WindowClass<Tag>::ClearCallbacks() {
		WindowProcedure::Subcallbacks_.clear();
	}

	template<LiteralString Tag>
	void WindowClass<Tag>::Initialize(HCURSOR cursor_) {
		if (Properties_.lpszClassName != nullptr) { return; }

		WindowProcedure::Subcallbacks_.clear();

		Properties_.lpfnWndProc = WindowProcedure::Callback;
		Properties_.hInstance = ::GetModuleHandle(nullptr);
		Properties_.hCursor = cursor_;
		Properties_.hbrBackground = ::GetSysColorBrush(::GetSysColor(COLOR_BACKGROUND));
		Properties_.lpszClassName = Tag();

		::RegisterClassEx(&Properties_);
	}

	//----	------	------	------	------	----//

	template<LiteralString Tag>
	constexpr WindowClass<Tag>::WindowClass() noexcept :
		Properties_{
			.cbSize{ sizeof(WNDCLASSEX) },
			.lpszClassName{ nullptr },
		} {}

	template<LiteralString Tag>
	constexpr WindowClass<Tag>::~WindowClass() noexcept {}
}

namespace Lumina::WinApp {
	void Window::Initialize(
		std::wstring_view className_,
		Config const& config_
	) {
		if (Handle_ != nullptr) { return; }

		ClassName_ = className_;
		Config_ = config_;
		Config_.ClientWidth = std::max<uint32_t>(Config_.ClientWidth, 1U);
		Config_.ClientHeight = std::max<uint32_t>(Config_.ClientHeight, 1U);
		AspectRatio_ = static_cast<float>(Config_.ClientWidth) / float(Config_.ClientHeight);

		RECT windowRect{
			0, 0,
			static_cast<int32_t>(Config_.ClientWidth),
			static_cast<int32_t>(Config_.ClientHeight)
		};
		// Calculate the actual window size.
		::AdjustWindowRect(&windowRect, Config_.Style, false);

		Handle_ = ::CreateWindow(
			ClassName_.data(),							// Class name
			Config_.Title.data(),						// Window title
			Config_.Style,								// Window style
			CW_USEDEFAULT,								// Pos X
			CW_USEDEFAULT,								// Pos Y
			windowRect.right - windowRect.left,			// Window width
			windowRect.bottom - windowRect.top,			// Window height
			nullptr,									// Handle to the parent window
			nullptr,									// Handle to the menu
			::GetModuleHandle(nullptr),					// Handle to the instance of the module
			nullptr										// Miscellaneous
		);

		::ShowWindow(Handle_, SW_SHOW);

		Utils::Debug::Logger::Default().Message<0U>(
			"New window created:\n- Name = {}\n- Title = {}\n- ClientWidth = {:d}\n- ClientHeight = {:d}\n",
			Utils::String::Convert(Config_.Name),
			Utils::String::Convert(Config_.Title),
			Config_.ClientWidth,
			Config_.ClientHeight
		);
	}

	constexpr Window::Window() noexcept {}
	constexpr Window::~Window() noexcept {}
}

export namespace Lumina::WinApp {
	template<typename WindowStyle>
		requires(std::is_same_v<WindowStyle, Window::STYLE> || std::is_same_v<WindowStyle, uint32_t>)
	constexpr uint32_t operator|(WindowStyle lhs_, Window::STYLE rhs_) {
		return static_cast<uint32_t>(lhs_) | static_cast<uint32_t>(rhs_);
	}
}