module;

#include<Windows.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.WinApp.Context;

import <cstdint>;
import <type_traits>;

import <memory>;
import <functional>;

import <vector>;

import <string>;
import <format>;

import Lumina.WinApp;
import Lumina.WinApp.RawInput;

import Lumina.Mixins;

import Lumina.Utils.String;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

namespace Lumina::WinApp {
	export using WindowConfig = Window::Config;
	export using WindowStyle = Window::STYLE;
}

namespace Lumina::WinApp {
	export class Context final : public NonCopyable<Context> {
	public:
		auto WindowInstance(std::wstring_view windowName_) const -> Window const&;
		auto RawInputContext(Window const& windowInst_) const -> RawInput const&;

		//----	------	------	------	------	----//

	public:
		void RegisterCallback(CallbackFunctor const& callback_);

		Window const& NewWindow(Window::Config const& windowConfig_);
		void DeleteWindow(HWND hWnd_);

		//----	------	------	------	------	----//

	public:
		int32_t ProcessMessage();

		//----	------	------	------	------	----//

	public:
		void Initialize(
			Window::Config const& mainWindowConfig_ = {
				.Name{ L"Main" },
				.Title{ L"Usus Magister Est Optimus" },
				.Style{ Window::STYLE::TitleBar | Window::STYLE::WindowMenu | Window::STYLE::MinimizeButton },
				.ClientWidth{ 1280U },
				.ClientHeight{ 720U },
			}
		);
		void Finalize() noexcept;

		//----	------	------	------	------	----//

	public:
		constexpr Context() noexcept = default;
		~Context() noexcept;

		//====	======	======	======	======	====//

	private:
		WindowClass<L"YYKKYK"> WindowClass_{};

		std::vector<UniPtr<Window>> Array_WindowInstances_{};
	};
}