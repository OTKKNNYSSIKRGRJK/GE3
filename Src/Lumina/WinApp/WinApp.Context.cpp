module;

#include<Windows.h>

module Lumina.WinApp.Context;

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

namespace Lumina::WinApp {
	namespace {
		class WindowEX : public Window {
		public:
			uint32_t ID{ 0xFFFFFFFFU };
			UniPtr<RawInput> RawInputContext{ nullptr };
		};
	}
}

namespace Lumina::WinApp {
	auto Context::WindowInstance(std::wstring_view windowName_) const -> Window const& {
		Window const* window{ nullptr };

		// Normally, there should be only a few window instance existing simultaneously,
		// so search in such a primitive way should be fine.
		for (auto const& windowInst : Array_WindowInstances_) {
			if (windowInst->Name() == windowName_) {
				window = windowInst.get();
				break;
			}
		}

		(window != nullptr) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<WinApp.Context> Failed to find Window \"{}\"!",
				Utils::String::Convert(windowName_)
			)
		};

		return *window;
	}

	auto Context::RawInputContext(Window const& windowInst_) const -> RawInput const& {
		return *(static_cast<WindowEX const&>(windowInst_).RawInputContext);
	}

	//----	------	------	------	------	----//

	void Context::RegisterCallback(CallbackFunctor const& callback_) {
		WindowClass_.RegisterCallback(callback_);
	}

	Window const& Context::NewWindow(Window::Config const& windowConfig_) {
		// There should be no multiple window instances with the same name.
		for (auto const& windowInst : Array_WindowInstances_) {
			(windowInst->Name() != windowConfig_.Name) ||
			Utils::Debug::ThrowIfFalse{
				std::format(
					"<WinApp.Context> A Window instance with the same name \"{}\" has been found!\n",
					Utils::String::Convert(windowConfig_.Name)
				)
			};
		}

		WindowEX& windowInst{
			*static_cast<WindowEX*>(
				Array_WindowInstances_.emplace_back(new WindowEX{}).get()
			)
		};
		windowInst.Initialize(WindowClass_.Name(), windowConfig_);
		windowInst.ID = static_cast<uint32_t>(Array_WindowInstances_.size()) - 1U;

		windowInst.RawInputContext = UniPtr<RawInput>{ new RawInput{} };
		windowInst.RawInputContext->Initialize(windowInst.Handle());
		RegisterCallback(windowInst.RawInputContext->Callback());

		return windowInst;
	}

	void Context::DeleteWindow(HWND hWnd_) {
		::DestroyWindow(hWnd_);
	}

	//----	------	------	------	------	----//

	int32_t Context::ProcessMessage() {
		static MSG msg{};

		// Deals with the messages from the window with the highest priority.
		// Exits the loop as soon as nothing is left in the message queue.
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT) { return 1; }
		}
		return 0;
	}

	//----	------	------	------	------	----//

	void Context::Initialize(Window::Config const& mainWindowConfig_) {
		WindowClass_.Initialize(::LoadCursor(nullptr, IDC_ARROW));

		NewWindow(mainWindowConfig_);
	}

	void Context::Finalize() noexcept {
		WindowClass_.ClearCallbacks();

		for (auto& windowInst : Array_WindowInstances_) {
			static_cast<WindowEX*>(windowInst.get())->RawInputContext->Finalize();

			::CloseWindow(windowInst->Handle());
			::DestroyWindow(windowInst->Handle());
		}
	}

	//----	------	------	------	------	----//

	Context::~Context() noexcept {
		Finalize();
	}
}