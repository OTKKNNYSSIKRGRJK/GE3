module;

#include<memory>

#include<Windows.h>

#include<d3d12.h>

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

export module Lumina;

import Lumina.Math;
import Lumina.Math.Quaternion;
import Lumina.Utils;

import Lumina.WinApp.Context;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

import Lumina.AssetManager;

import Lumina.Utils.ImGui;

import Lumina.Utils.Time;

import Lumina.Editor.DX12;

namespace {
	void SetImGuiAppearance() {
		//ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 12.0f);
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
}

namespace Lumina {
	export class Context {
	public:
		bool Run();

	public:
		void Initialize();
		void Finalize();

	private:
		Lumina::WinApp::Context WinAppContext_{};
		Lumina::WinApp::RawInput const* MainWindowRawInput_{ nullptr };
		Lumina::DX12::Context DXContext_{};
		Lumina::AssetManager AssetManager_{};

		Lumina::DX12::CommandAllocator CmdAllocator_{};
		Lumina::DX12::CommandList CmdList_{};
	};

	bool Context::Run() {
		if (WinAppContext_.ProcessMessage() == 0) {
			ID3D12DescriptorHeap* descriptorHeaps[]{ DXContext_.GlobalDescriptorHeap().Get() };

			[[maybe_unused]] auto& directQueue{ DXContext_.DirectQueue() };

			auto const& keyboard = MainWindowRawInput_->Keyboard();
			auto const& mouse = MainWindowRawInput_->Mouse();

			DXContext_.BeginFrame(CmdList_);

			auto rtv{ DXContext_.SwapChain().BackBufferRTVCPUHandle() };
			auto dsv{ DXContext_.SwapChain().DSVCPUHandle() };

			D3D12_RENDER_PASS_BEGINNING_ACCESS beginning_RTClear{
				.Type{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR },
				.Clear{
					.ClearValue{
						.Format{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
						.Color{ 0.0f, 0.0f, 0.0f, 0.0f, },
					},
				},
			};

			D3D12_RENDER_PASS_BEGINNING_ACCESS beginning_DSClear{
				.Type{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR },
				.Clear{
					.ClearValue{
						.Format{ DXGI_FORMAT_D24_UNORM_S8_UINT },
						.DepthStencil{
							.Depth{ 1.0f },
						},
					},
				},
			};

			D3D12_RENDER_PASS_ENDING_ACCESS ending_Preserve{
				.Type{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE },
			};

			[[maybe_unused]] D3D12_RENDER_PASS_RENDER_TARGET_DESC renderTargetDesc{
				.cpuDescriptor{ rtv },
				.BeginningAccess{ beginning_RTClear },
				.EndingAccess{ ending_Preserve },
			};

			[[maybe_unused]] D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthStencilDesc{
				.cpuDescriptor{ dsv },
				.DepthBeginningAccess{ beginning_DSClear },
				.StencilBeginningAccess{.Type{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS } },
				.DepthEndingAccess{ ending_Preserve },
				.StencilEndingAccess{.Type{ D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS } },
			};

			static_cast<ID3D12GraphicsCommandList4*>(CmdList_.Get())->BeginRenderPass(
				1U,
				&renderTargetDesc,
				&depthStencilDesc,
				D3D12_RENDER_PASS_FLAG_NONE
			);

			//----	------	------	------	------	----//

			Lumina::Utils::ImGuiManager::BeginFrame();
			CmdList_->SetDescriptorHeaps(1U, descriptorHeaps);

			ImGui::Begin("Raw Input Test");
			ImGui::SeparatorText("Keyboard");
			ImGui::BulletText("R SHIFT = %d", keyboard.IsPressed(Lumina::WinApp::KEY::SHIFT_RIGHT));
			ImGui::BulletText("L CTRL = %d", keyboard.IsPressed(Lumina::WinApp::KEY::CTRL_LEFT));
			ImGui::BulletText("R CTRL = %d", keyboard.IsPressed(Lumina::WinApp::KEY::CTRL_RIGHT));

			ImGui::SeparatorText("Mouse");
			ImGui::BulletText("PosX = %d", mouse.PosX());
			ImGui::BulletText("PosY = %d", mouse.PosY());
			ImGui::BulletText("DeltaX = %d", mouse.DeltaX());
			ImGui::BulletText("DeltaY = %d", mouse.DeltaY());
			ImGui::BulletText("LeftButton = %d", mouse.LeftButton());
			ImGui::BulletText("RightButton = %d", mouse.RightButton());
			ImGui::BulletText("Wheel = %d", mouse.Wheel());
			ImGui::BulletText("Wheel = %d", mouse.DeltaWheel());
			ImGui::End();

			ImGui::Begin("Quaternion");
			static Lumina::Quaternion pose{};
			static Lumina::Vec4 vec{ 0.0f, 0.0f, 0.0f, 1.0f };
			static Lumina::Float3 poseAxis{ 1.0f, 0.0f, 0.0f };
			static float poseAngle{ 0.0f };
			ImGui::DragFloat3("Axis", &poseAxis.x, 0.01f);
			ImGui::DragFloat("Angle", &poseAngle, 0.01f);
			ImGui::DragFloat4("Vec", vec(), 0.01f);
			pose = Lumina::Quaternion::RotateAbout(poseAxis, poseAngle);
			ImGui::DragFloat4("Rotate", &pose.x, 0.0f);
			ImGui::Text("Re = %f", pose.Re(), 0.0f);
			ImGui::Text("Im = (%f, %f, %f)", pose.Im().x, pose.Im().y, pose.Im().z, 0.0f);
			ImGui::DragFloat4("Rotated Vec", Lumina::Quaternion::Rotate(vec, pose)(), 0.0f);
			ImGui::End();

			//ImGui::ShowStyleEditor();

			static_cast<ID3D12GraphicsCommandList4*>(CmdList_.Get())->EndRenderPass();

			//----	------	------	------	------	----//

			D3D12_RENDER_PASS_BEGINNING_ACCESS beginning_Preserve{
				.Type{ D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE },
			};

			[[maybe_unused]] D3D12_RENDER_PASS_RENDER_TARGET_DESC renderTargetDesc2{
				.cpuDescriptor{ rtv },
				.BeginningAccess{ beginning_Preserve },
				.EndingAccess{ ending_Preserve },
			};

			static_cast<ID3D12GraphicsCommandList4*>(CmdList_.Get())->BeginRenderPass(
				1U,
				&renderTargetDesc2,
				nullptr,
				D3D12_RENDER_PASS_FLAG_NONE
			);

			Lumina::Utils::ImGuiManager::EndFrame(CmdList_);

			static_cast<ID3D12GraphicsCommandList4*>(CmdList_.Get())->EndRenderPass();

			//----	------	------	------	------	----//

			DXContext_.EndFrame(CmdAllocator_, CmdList_);

			//----	------	------	------	------	----//

			if (keyboard.IsPressed(Lumina::WinApp::KEY::ESC)) {
				::SendMessage(WinAppContext_.WindowInstance(L"Main").Handle(), WM_CLOSE, 0, 0);
			}

			return 1;
		}

		return 0;
	}

	void Context::Initialize() {
		Lumina::WinApp::WindowConfig mainWindowConfig_{
			.Name{ L"Main" },
			.Title{ L"Usus Magister Est Optimus" },
			.Style{
				Lumina::WinApp::WindowStyle::TitleBar |
				Lumina::WinApp::WindowStyle::WindowMenu |
				Lumina::WinApp::WindowStyle::MinimizeButton
			},
			.ClientWidth{ 1280U },
			.ClientHeight{ 720U },
		};
		WinAppContext_.Initialize(mainWindowConfig_);
		auto const& mainWindow{ WinAppContext_.WindowInstance(L"Main") };
		MainWindowRawInput_ = &WinAppContext_.RawInputContext(mainWindow);

		//----	------	------	------	------	----//

		DXContext_.Initialize(WinAppContext_);
		auto const& device{ DXContext_.Device() };

		auto const& gpuDH{ DXContext_.GlobalDescriptorHeap() };

		CmdAllocator_.Initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		CmdList_.Initialize(device, CmdAllocator_);

		AssetManager_.Initialize(DXContext_);

		std::vector<uint32_t> texIDs{};
		AssetManager_.Graphics().LoadImageTextures(
			texIDs,
			{
				{ "Star1", "Assets/Star1.png" },
				{ "UVChecker", "Assets/uvChecker.png" },
				{ "CLIMATE", "Assets/CLIMATE.png" },
				{ "OCEAN", "Assets/OCEAN.png" },
			}
		);

		auto texSRVTable{ gpuDH.Allocate(32U) };
		//texIDs[2] = texIDs[1];
		for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
			device->CopyDescriptorsSimple(
				1U,
				texSRVTable.CPUHandle(idx),
				AssetManager_.Graphics().CPUHandle(texIDs.at(idx)),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
		}

		//auto handle_Stream = AssetManager_.Audio().LoadFromFile("Assets/test.mp3");
		//AssetManager_.Audio().Play(handle_Stream, true, 1.0f);

		//----	------	------	------	------	----//

		auto const& swapChain{ DXContext_.SwapChain() };
		Lumina::Utils::ImGuiManager::Initialize(mainWindow.Handle(), device, swapChain, gpuDH);
		WinAppContext_.RegisterCallback(Lumina::Utils::ImGuiManager::WindowProcedure);
		SetImGuiAppearance();
	}

	void Context::Finalize() {		
		DXContext_.DirectQueue().SignalAndCPUWait();

		#if defined(_DEBUG)
		Lumina::Utils::ImGuiManager::Finalize();
		#endif

		AssetManager_.Finalize();

		WinAppContext_.Finalize();
	}
}