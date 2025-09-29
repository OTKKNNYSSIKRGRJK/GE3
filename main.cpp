#include<functional>

#include<vector>

#include<memory>
#include<chrono>

#include<string>
#include<format>

#include<fstream>

#include<Windows.h>

#include<d3d12.h>

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

import <cstdint>;
import <type_traits>;

import Lumina.Math;
import Lumina.Math.Quaternion;
import Lumina.Utils;

namespace LU = Lumina::Utils;

import Lumina.WinApp.Context;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

import Lumina.AssetManager;

//import Lumina.BitonicSort;
//import Lumina.GPUParticle;
//import Lumina.PerlinNoiseTest;
import Lumina.ProceduralTerrain;

import Lumina.Utils.ImGui;

import Lumina.Utils.Time;

import Lumina.Editor.DX12;

void Print(const Lumina::Mat4& m_) {
	auto str{
		std::format(
			"{} {} {} {}\n{} {} {} {}\n{} {} {} {}\n{} {} {} {}\n",
			m_[0][0], m_[0][1], m_[0][2], m_[0][3],
			m_[1][0], m_[1][1], m_[1][2], m_[1][3],
			m_[2][0], m_[2][1], m_[2][2], m_[2][3],
			m_[3][0], m_[3][1], m_[3][2], m_[3][3]
		)
	};

	OutputDebugStringA(str.data());
}

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

int32_t WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	/*auto& engine{ Lumina::Engine::Instance() };
	Lumina::WindowConfig windowConfig{
		.Name { "Main" },
		.Title{ L"最高にクールなタイトル" },
		.Style{ WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX },
		.ClientWidth{ 1280 },
		.ClientHeight{ 720 },
	};
	engine << windowConfig;
	
	engine.Initialize();

	auto const& mainWindow{ engine.MainWindow() };*/

	// Initialization of WinApp context

	Lumina::WinApp::Context winAppContext{};
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
	winAppContext.Initialize(mainWindowConfig_);
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

	/*Lumina::Vec3 cameraRotate{ 0.0f, 0.0f, 0.0f };
	Lumina::Vec3 cameraTranslate{ 0.0f, 0.0f, -30.0f };
	Lumina::Mat4 camera{
		Lumina::Mat4::SRT(
			{ 1.0f, 1.0f, 1.0f },
			cameraRotate,
			cameraTranslate
		)
	};
	Lumina::Mat4 view{ camera.Inv() };
	Lumina::Mat4 projection{
		Lumina::Mat4::PerspectiveFOV(
			0.45f,
			static_cast<float>(mainWindow.ClientWidth()) / static_cast<float>(mainWindow.ClientHeight()),
			0.1f,
			200.0f
		)
	};
	Lumina::Mat4 vp{ view * projection };

	uint64_t vpBufferSize = (((sizeof(Lumina::Mat4) - 1LLU) >> 8U) + 1LLU) << 8U;
	Lumina::DX12::UploadBuffer vpUploadBuf{};
	vpUploadBuf.Initialize(device, vpBufferSize, "ViewProjection");
	vpUploadBuf.Store(&vp, sizeof(Lumina::Mat4), 0U);

	auto vpCBVTable{ gpuDH.Allocate(32U) };
	D3D12_CONSTANT_BUFFER_VIEW_DESC vpCBVDesc{ Lumina::DX12::CBV::Desc{ vpUploadBuf } };
	device->CreateConstantBufferView(&vpCBVDesc, vpCBVTable.CPUHandle(0U));*/

	//----	------	------	------	------	----//

	auto config{ LU::LoadFromFile<NLohmannJSON>("config.json") };

	//----	------	------	------	------	----//

	/*auto&& obj_Axis{ LU::LoadFromFile<LU::WavefrontOBJ>("axis.obj", "Assets")};
	auto modelData_Axis{ LU::Model{ obj_Axis } };
	auto vertexBuffer = Lumina::DX12::VertexBuffer<LU::Model::Vertex>::Create(
		device,
		static_cast<uint32_t>(modelData_Axis.Vertices.size()),
		"Axis"
	);
	std::memcpy(
		vertexBuffer(),
		modelData_Axis.Vertices.data(),
		sizeof(LU::Model::Vertex) * modelData_Axis.Vertices.size()
	);

	//----	------	------	------	------	----//

	auto vertexShader{
		Lumina::DX12::Shader::Create(
			*shaderCompiler,
			L"Assets/Shaders/Basic.VS.hlsl",
			L"vs_6_6",
			L"main"
		)
	};
	auto pixelShader{
		Lumina::DX12::Shader::Create(
			*shaderCompiler,
			L"Assets/Shaders/Basic.PS.hlsl",
			L"ps_6_6",
			L"main"
		)
	};
	[[maybe_unused]] auto rs{
		Lumina::DX12::LoadRootSignature(
			device,
			config.at("Basic RS"),
			"Basic RS"
		)
	};
	[[maybe_unused]] auto graphicsPSO{
		Lumina::DX12::LoadGraphicsPipelineState(
			device,
			rs, vertexShader, pixelShader,
			config.at("Basic PSO"),
			"Basic PSO"
		)
	};*/

	//----	------	------	------	------	----//

	Lumina::AssetManager assetMngr{};
	assetMngr.Initialize(dx12Context);
	std::vector<uint32_t> texIDs{};

	assetMngr.Graphics().LoadImageTextures(
		texIDs,
		{
			{ "Star1", "Assets/Star1.png" },
			{ "UVChecker", "Assets/uvChecker.png" },
			{ "CLIMATE", "Assets/CLIMATE.png" },
			{ "OCEAN", "Assets/OCEAN.png" },
		}
	);

	auto texSRVTable{ gpuDH.Allocate(32U) };
	texIDs[2] = texIDs[1];
	for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
		device->CopyDescriptorsSimple(
			1U,
			texSRVTable.CPUHandle(idx),
			assetMngr.Graphics().CPUHandle(texIDs.at(idx)),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
	}

	//auto handle_Stream = assetMngr.Audio().LoadFromFile("Assets/test.mp3");
	//assetMngr.Audio().Play(handle_Stream, true, 1.0f);

	//----	------	------	------	------	----//

	auto const& swapChain{ dx12Context.SwapChain() };
	Lumina::Utils::ImGuiManager::Initialize(mainWindow.Handle(), device, swapChain, gpuDH);
	winAppContext.RegisterCallback(Lumina::Utils::ImGuiManager::WindowProcedure);
	SetImGuiAppearance();

	//----	------	------	------	------	----//

	Lumina::DX12::CommandQueue computeQueue{};
	computeQueue.Initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE, "ComputeQueue");
	//Lumina::DX12::Test::Foo(device, csuHeap, *shaderCompiler, config.at("ComputeShaderTest RS"));

	[[maybe_unused]] ID3D12DescriptorHeap* descriptorHeaps[]{ gpuDH.Get() };

	/*Lumina::Test::ParticleManager pm{};
	pm.Initialize(device, cmdQueue, computeQueue, csuHeap, *shaderCompiler);

	constexpr float inv_0xFFF{ 1.0f / 4095.0f };
	[[maybe_unused]] constexpr float rndAngleFactor{ std::numbers::pi_v<float> / 12.0f };
	[[maybe_unused]] constexpr float piOver180{ std::numbers::pi_v<float> / 180.0f };
	constexpr auto rndAngle{
		[rndAngleFactor]()->float {
			return static_cast<float>(Lumina::RndEngine() & 0xFFF) * rndAngleFactor;
		}
	};

	float initPos[3]{};
	float ts[3]{ 0.0f, 0.0f, 0.0f, };*/

	//----	------	------	------	------	----//

	//Lumina::PerlinNoiseTest pnTest{};
	 
	auto& directQueue{ dx12Context.DirectQueue() };
	/*Lumina::Terrain terrain{};
	terrain.Initialize(dx12Context);
	terrain.UpdateElevation(directQueue);
	terrain.UpdateTemperature(directQueue);
	terrain.UpdatePrecipitation(directQueue);*/

	//----	------	------	------	------	----//

	std::chrono::steady_clock::time_point timeStamps[2]{};
	int cnt_TimeStamp = 0;
	timeStamps[cnt_TimeStamp] = std::chrono::steady_clock::now();

	Lumina::Utils::Horometer horometer{};
	horometer.Initialize();

	Lumina::Editor::DirectX12 dxEditor{};
	dxEditor.Initialize();

	/*Game::Scene_InGame scene_InGame{};
	scene_InGame.Initialize(dx12Context);*/

	auto const& keyboard = mainWindowRawInput.Keyboard();
	auto const& mouse = mainWindowRawInput.Mouse();

	while (winAppContext.ProcessMessage() == 0) {
		dx12Context.BeginFrame(cmdList);
		
		//----	------	------	------	------	----//

		Lumina::Utils::ImGuiManager::BeginFrame();

		//----	------	------	------	------	----//

		/*ImGui::Begin("Camera");
		ImGui::DragFloat3("Rotate##Camera", cameraRotate(), 0.01f);
		ImGui::DragFloat3("Translate##Camera", cameraTranslate(), 0.01f);
		camera = Lumina::Mat4::SRT(
			{ 1.0f, 1.0f, 1.0f },
			cameraRotate,
			cameraTranslate
		);
		view = camera.Inv();
		vp = view * projection;
		vpUploadBuf.Store(&vp, sizeof(Lumina::Mat4), 0U);
		ImGui::End();*/

		//----	------	------	------	------	----//

		/*initPos[0] = std::sin(ts[0] * (-1.25f)) * 1.5f;
		initPos[1] = std::cos(ts[1] * 0.95f) * 1.5f;
		initPos[2] = std::sin(ts[2] * 1.6f) * 1.5f;
		ts[0] += (Lumina::RndEngine() & 15) * 0.0001f - 0.005f;
		ts[1] += (Lumina::RndEngine() & 15) * 0.0001f - 0.005f;
		ts[2] += (Lumina::RndEngine() & 15) * 0.0001f - 0.005f;

		for (int i = 0; i < 256; ++i) {
			const float theta = rndAngle();
			const float cosTheta = std::cos(theta);
			const float sinTheta = std::sin(theta);
			const auto phi = rndAngle();
			const float cosPhi = std::cos(phi);
			const float sinPhi = std::sin(phi);

			Lumina::Test::Particle initData{
				.Position{
					initPos[0],
					initPos[1],
					initPos[2],
				},
				.Velocity{
					cosTheta * cosPhi * 0.015f,
					sinPhi * 0.015f,
					sinTheta * cosPhi * 0.015f
				},
				.Color{
					1.0f,
					(Lumina::RndEngine() & 0x7FF) * inv_0xFFF,
					(Lumina::RndEngine() & 0x7FF) * inv_0xFFF,
					0.0625f
				},
				.Duration{ (Lumina::RndEngine() & 0xFFF) + 1U },
			};
			pm.BatchEmission(1U, initData);
			initData.Position[0] = -initPos[1];
			initData.Position[1] = initPos[2];
			initData.Position[2] = initPos[0];
			pm.BatchEmission(1U, initData);
			initData.Position[0] = initPos[2];
			initData.Position[1] = initPos[0];
			initData.Position[2] = -initPos[1];
			pm.BatchEmission(1U, initData);
			initData.Position[0] = initPos[0];
			initData.Position[1] = -initPos[2];
			initData.Position[2] = initPos[1];
			pm.BatchEmission(1U, initData);
		}
		pm.DispatchEmissions(computeQueue, csuHeap);
		pm.Update(computeQueue, csuHeap);
		pm.Render(cmdList, csuHeap, vpCBVTable, texSRVTable);*/

		//----	------	------	------	------	----//

		/*vBufData[0].Color.g = std::abs(std::sin(t));
		vBufData[1].Color.b = std::abs(std::sin(t * 1.1f));
		vBufData[2].Color.r = std::abs(std::sin(t * 0.45f));
		t += 0.01f;*/

		//----	------	------	------	------	----//

		/*terrain.Update(directQueue);
		terrain.Render(cmdList, gpuDH, vpCBVTable, texSRVTable);*/

		cmdList->SetDescriptorHeaps(1U, descriptorHeaps);

		/*scene_InGame.Update(winAppContext, cmdList);
		scene_InGame.Render(dx12Context, cmdList);*/

		/*
		cmdList->SetPipelineState(graphicsPSO.Get());
		cmdList->SetGraphicsRootSignature(rs.Get());
		cmdList->SetGraphicsRootDescriptorTable(0U, texSRVTable.GPUHandle(0U));
		cmdList->SetGraphicsRootDescriptorTable(1U, cbvTable.GPUHandle(0U));

		const D3D12_VERTEX_BUFFER_VIEW vbv{ Lumina::DX12::VBV{ vertexBuffer } };
		cmdList->IASetVertexBuffers(0U, 1U, &vbv);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(static_cast<uint32_t>(modelData_Axis.Vertices.size()), 1U, 0U, 0U);*/

		horometer.Update();

		auto deltaTime = horometer.DeltaTime();
		auto fps = 1000.0f / static_cast<float>(deltaTime.count());

		ImGui::Begin("Performance");
		ImGui::Text("Frame Time = %u", horometer.DeltaTime().count());
		ImGui::Text("FPS = %f", fps);
		ImGui::Text("FPS = %f", ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::Begin("Raw Input Test");

		ImGui::SeparatorText("Keyboard");
		ImGui::BulletText("L SHIFT = %d", keyboard.IsPressed(Lumina::WinApp::KEY::SHIFT_LEFT));
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

		ImGui::ShowStyleEditor();

		dxEditor.Update();

		//pnTest.Update();

		Lumina::Utils::ImGuiManager::EndFrame(cmdList);

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