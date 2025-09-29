module;

#include<Windows.h>

#include<External/ImGui/backends/imgui_impl_dx12.h>
#include<External/ImGui/backends/imgui_impl_win32.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.Utils.ImGui;

export import <External/ImGui/imgui.h>;
export import <External/ImGui/imgui_internal.h>;

import Lumina.DX12;

import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

export namespace Lumina::Utils {
	class ImGuiManager {
	public:
		static void Initialize(
			HWND hWnd_,
			DX12::GraphicsDevice const& device_,
			DX12::FrameBufferSwapChain const& swapChain_,
			DX12::DescriptorHeap const& srvHeap_
		);
		static void Finalize();

		static void BeginFrame();
		static void EndFrame(const DX12::CommandList& cmdList_);

		static LRESULT CALLBACK WindowProcedure(
			HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_
		);
	};
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

module : private;

extern "C++" IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_
);

namespace Lumina::Utils {
	void ImGuiManager::Initialize(
		HWND hWnd_,
		DX12::GraphicsDevice const& device_,
		DX12::FrameBufferSwapChain const& swapChain_,
		DX12::DescriptorHeap const& srvHeap_
	) {
		auto& logger{ Debug::Logger::Default() };
		logger.Message<0U>("Initializing ImGui...\n");

		auto&& srvForImGui{ srvHeap_.Allocate(1U) };
		logger.Message<0U>(
			"- SRV for ImGui : CPUHandle = {}, GPUHandle = {}\n",
			srvForImGui.CPUHandle(0U).ptr, srvForImGui.GPUHandle(0U).ptr
		);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(hWnd_);
		ImGui_ImplDX12_Init(
			device_.Get(),
			swapChain_.Desc().BufferCount,
			swapChain_.RTVDesc().Format,
			srvHeap_.Get(),
			srvForImGui.CPUHandle(0U),
			srvForImGui.GPUHandle(0U)
		);

		logger.Message<0U>("ImGui initialization completed.\n");
	}

	void ImGuiManager::Finalize() {
		auto& logger{ Debug::Logger::Default() };
		logger.Message<0U>("Finalizing ImGui...\n");

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		logger.Message<0U>("ImGui finalization completed.\n");
	}

	void ImGuiManager::BeginFrame() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiManager::EndFrame(const DX12::CommandList& cmdList_) {
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList_.Get());
	}

	LRESULT CALLBACK ImGuiManager::WindowProcedure(
		HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_
	) {
		return ImGui_ImplWin32_WndProcHandler(hWnd_, msg_, wParam_, lParam_);
	}
}