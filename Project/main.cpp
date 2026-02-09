#include<functional>

#include<vector>

#include<memory>
#include<chrono>

#include<string>
#include<format>

#include<fstream>

#include<Windows.h>

#include<d3d12.h>

import <cstdint>;
import <type_traits>;

import Lumina.Math;
import Lumina.Math.Quaternion;
import Lumina.Utils;

import Lumina.Container.List;

import Lumina.WinApp.Context;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

import Lumina.AssetManager;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

import Lumina.Utils.Time;

import Game.Scene.InGame;

namespace {
	#if defined(_DEBUG)
	void SetImGuiAppearance() {
		ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/Fonts/AnonymousPro/AnonymousPro-Regular.ttf", 12.0f);
		
		ImGuiStyle& style{ ImGui::GetStyle() };
		style.WindowRounding = 3.0f;
		style.ChildRounding = 3.0f;
		style.PopupRounding = 3.0f;
		style.FrameRounding = 3.0f;
		style.FrameBorderSize = 1.0f;
		style.GrabRounding = 3.0f;
		style.TabBorderSize = 1.0f;
		style.TabRounding = 3.0f;
		style.SeparatorTextBorderSize = 1.0f;
		style.SeparatorTextPadding.y = 6.0f;
		style.CellPadding.y = 6.0f;

		ImVec4* colors{ style.Colors };
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.02f, 0.02f, 0.03f, 0.94f };
		colors[ImGuiCol_ChildBg] = ImVec4{ 0.02f, 0.02f, 0.03f, 0.06f };
		colors[ImGuiCol_PopupBg] = ImVec4{ 0.06f, 0.08f, 0.10f, 0.94f };
		colors[ImGuiCol_Border] = ImVec4{ 0.71f, 0.54f, 0.13f, 0.25f };
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.39f, 0.63f, 0.87f, 0.19f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.28f, 0.41f, 0.52f, 0.19f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.39f, 0.63f, 0.87f, 0.38f };
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.00f, 0.01f, 0.02f, 1.00f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.02f, 0.09f, 0.16f, 1.00f };
		colors[ImGuiCol_CheckMark] = ImVec4{ 0.50f, 0.58f, 0.68f, 1.00f };
		colors[ImGuiCol_SliderGrab] = ImVec4{ 0.39f, 0.56f, 0.61f, 0.75f };
		colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.46f, 0.71f, 0.79f, 0.75f };
		colors[ImGuiCol_Button] = ImVec4{ 0.01f, 0.06f, 0.08f, 0.75f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.03f, 0.09f, 0.18f, 0.25f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.12f, 0.38f, 0.71f, 0.50f };
		colors[ImGuiCol_SeparatorHovered] = ImVec4{ 0.10f, 0.24f, 0.40f, 0.78f };
		colors[ImGuiCol_SeparatorActive] = ImVec4{ 0.05f, 0.24f, 0.45f, 1.00f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.20f, 0.47f, 0.59f, 0.75f };
		colors[ImGuiCol_Tab] = ImVec4{ 0.05f, 0.20f, 0.25f, 0.13f };
		colors[ImGuiCol_TabSelected] = ImVec4{ 0.07f, 0.29f, 0.44f, 0.63f };
		colors[ImGuiCol_TabSelectedOverline] = ImVec4{ 0.07f, 0.29f, 0.44f, 0.63f };
		colors[ImGuiCol_TableHeaderBg] = ImVec4{ 0.01f, 0.07f, 0.11f, 0.50f };
		colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 0.24f, 0.23f, 0.21f, 0.06f };
		colors[ImGuiCol_Header] = ImVec4{ 0.12f, 0.33f, 0.47f, 0.31f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.07f, 0.29f, 0.44f, 0.63f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.20f, 0.47f, 0.59f, 0.75f };
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.00f, 0.01f, 0.02f, 0.25f };
		colors[ImGuiCol_DragDropTarget] = ImVec4{ 0.80f, 0.68f, 0.20f, 0.75f };
		colors[ImGuiCol_TableBorderStrong] = ImVec4{ 0.76f, 0.57f, 0.20f, 0.38f };
		colors[ImGuiCol_TableBorderLight] = ImVec4{ 0.38f, 0.38f, 0.34f, 0.25f };
	}
	#endif

	struct DirectionalLight {
		Lumina::Float3 Color;
		float Intensity;
		Lumina::Float3 Dir;
	};
}

int32_t WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	Lumina::WinApp::Context winAppContext{};
	Lumina::WinApp::WindowConfig mainWindowConfig{
		.Name{ L"Main" },
		.Title{ L"LE2C_コウ_シキン_草" },
		.Style{
			Lumina::WinApp::WindowStyle::TitleBar |
			Lumina::WinApp::WindowStyle::WindowMenu |
			Lumina::WinApp::WindowStyle::MinimizeButton
		},
		.ClientWidth{ 1280U },
		.ClientHeight{ 720U },
	};
	winAppContext.Initialize(mainWindowConfig);
	auto const& mainWindow{ winAppContext.WindowInstance(L"Main") };
	auto const& mainWindowRawInput{ winAppContext.RawInputContext(mainWindow) };

	//----	------	------	------	------	----//

	Lumina::DX12::Context dx12Context{};
	dx12Context.Initialize(winAppContext);
	auto const& device{ dx12Context.Device() };

	auto const& gpuDH{ dx12Context.GlobalDescriptorHeap() };

	Lumina::DX12::CommandAllocator cmdAllocator{};
	cmdAllocator.Initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	Lumina::DX12::CommandList cmdList{};
	cmdList.Initialize(device, cmdAllocator);

	//----	------	------	------	------	----//

	Lumina::AssetManager assetMngr{};
	assetMngr.Initialize(dx12Context);

	//----	------	------	------	------	----//

	DirectionalLight dirLight{
		.Color{ 1.0f, 1.0f, 1.0f },
		.Intensity{ 1.0f },
		.Dir{ 1.0f, 1.0f, 1.0f },
	};
	Lumina::DX12::UploadBuffer dirLightBuf{};
	dirLightBuf.Initialize(device, (sizeof(decltype(dirLight)) + 0xFF) & ~0xFF);
	dirLightBuf.Store(&dirLight, sizeof(decltype(dirLight)), 0LLU);
	Lumina::DX12::DescriptorTable lightGlobalTable{};
	gpuDH.Allocate(lightGlobalTable, 16U);
	Lumina::DX12::CBV::Create(device, lightGlobalTable.CPUHandle(0U), dirLightBuf);

	//----	------	------	------	------	----//

	[[maybe_unused]] auto const& swapChain{ dx12Context.SwapChain() };

	#if defined(_DEBUG)
	Lumina::Utils::ImGuiManager::Initialize(mainWindow.Handle(), device, swapChain, gpuDH);
	winAppContext.RegisterCallback(Lumina::Utils::ImGuiManager::WindowProcedure);
	SetImGuiAppearance();
	#endif

	//----	------	------	------	------	----//

	ID3D12DescriptorHeap* descriptorHeaps[]{ gpuDH.Get() };

	//----	------	------	------	------	----//
	 
	auto& directQueue{ dx12Context.DirectQueue() };

	//----	------	------	------	------	----//

	Lumina::Utils::Horometer horometer{};
	horometer.Initialize();

	[[maybe_unused]] auto const& keyboard = mainWindowRawInput.Keyboard();
	[[maybe_unused]] auto const& mouse = mainWindowRawInput.Mouse();

	//----	------	------	------	------	----//

	std::unique_ptr<Game::Scene::InGame> scene_InGame{ std::make_unique<Game::Scene::InGame>() };
	scene_InGame->Initialize(dx12Context, assetMngr);

	//----	------	------	------	------	----//

	while (winAppContext.ProcessMessage() == 0) {
		dx12Context.BeginFrame(cmdList);

		#if defined(_DEBUG)
		Lumina::Utils::ImGuiManager::BeginFrame();
		#endif

		//----	------	------	------	------	----//

		cmdList->SetDescriptorHeaps(1U, descriptorHeaps);

		//----	------	------	------	------	----//

		static Lumina::Mat4 const orthoProj{
			Lumina::Mat4::Orthographic(
				0.0f, 1280.0f,
				0.0f, 720.0f,
				0.1f, 100.0f
			)
		};

		scene_InGame->Update(dx12Context, cmdList, mainWindowRawInput);
		scene_InGame->Render(dx12Context, cmdList);

		//----	------	------	------	------	----//

		horometer.Update();

		//----	------	------	------	------	----//

		#if defined(_DEBUG)
		auto rtv{ swapChain.BackBufferRTVCPUHandle() };
		cmdList->OMSetRenderTargets(1U, &rtv, false, nullptr);
		Lumina::Utils::ImGuiManager::EndFrame(cmdList);
		#endif

		//----	------	------	------	------	----//

		dx12Context.EndFrame(cmdAllocator, cmdList);

		//----	------	------	------	------	----//

		if (keyboard.IsPressed(Lumina::WinApp::KEY::ESC)) {
			::SendMessage(mainWindow.Handle(), WM_CLOSE, 0, 0);
		}
	}

	directQueue.SignalAndCPUWait();

	#if defined(_DEBUG)
	Lumina::Utils::ImGuiManager::Finalize();
	#endif

	assetMngr.Finalize();

	winAppContext.Finalize();

	return 0;
}