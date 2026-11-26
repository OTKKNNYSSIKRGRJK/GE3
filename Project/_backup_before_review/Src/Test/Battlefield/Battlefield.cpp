module Game.Battlefield;

import nlohmann.json;

import Lumina.Utils.Data;
//import Lumina.Utils.ImGui;

import Lumina.Math;

import Lumina.DX12.Aux;

namespace Game {
	namespace {
		auto& rndEngine{ Lumina::Random::Generator() };
		float const Inv_0xFFFFFFFF{ 1.0f / static_cast<float>(0xFFFFFFFFU) };

		bool UpdateWaveSource(WaveSource& wavSrc_) {
			//wavSrc_.Position.x += (rndEngine() * Inv_0xFFFFFFFF - 0.5f) * 0.01f;
			//wavSrc_.Position.y += (rndEngine() * Inv_0xFFFFFFFF - 0.5f) * 0.01f;
			//wavSrc_.Position.x -= std::sin(wavSrc_.Timer * 0.005f) * 0.0001f;
			//wavSrc_.Position.y += std::cos(wavSrc_.Timer * 0.005f) * 0.0001f;
			wavSrc_.Timer += 1.0f;
			//return true;
			return wavSrc_.Timer < 30.0f;
		}

		struct PlaneVertex {
			Lumina::Float4 Position;
			Lumina::Float2 TexCoord;
		};
	}

	auto Battlefield::Borders() noexcept -> Lumina::Rect& { return Borders_; }
	auto Battlefield::Borders() const noexcept -> Lumina::Rect const& { return Borders_; }
	auto Battlefield::VisualBorders() noexcept -> Lumina::Rect& { return VisualBorders_; }
	auto Battlefield::VisualBorders() const noexcept -> Lumina::Rect const& { return VisualBorders_; }

	void Battlefield::OnHitWall(
		uint32_t idx_Wall_,
		Lumina::Vec3 const& worldPos_,
		float strength_
	) {
		if (!WaveSources_.IsFull()) {
			auto& waveSrc{ WaveSources_.New() };
			waveSrc.WallID = idx_Wall_;
			Lumina::Vec4&& localPos{ Lumina::Vec4{ worldPos_ } *Walls_[idx_Wall_]->WorldToLocal };
			// [-1, 1] -> [0, 1]
			waveSrc.Position.x = (localPos.x + 1.0f) * 0.5f;
			// [-1, 1] -> [1, 0]
			waveSrc.Position.y = (-localPos.y + 1.0f) * 0.5f;
			waveSrc.Amplitude = strength_;
			waveSrc.PhaseOffset = 1.5f;
			waveSrc.AngularFrequency = 0.125f;
			waveSrc.Timer = 0.0f;
			waveSrc.Update = UpdateWaveSource;
		}
	}

	void Battlefield::Update(Lumina::DX12::Context const& dx12Context_) {
		/*OnHitWall(
			rndEngine() % 4U,
			{ rndEngine() * Inv_0xFFFFFFFF * 120.0f - 60.0f, 0.0f, rndEngine() * Inv_0xFFFFFFFF * 120.0f - 60.0f },
			20.0f
		);*/
		/*if (!WaveSources_.IsFull()) {
			auto& waveSrc{ WaveSources_.New() };
			waveSrc.WallID = rndEngine() % 4U;
			waveSrc.Position.x = 0.5f;
			waveSrc.Position.y = 0.5f;
			waveSrc.Amplitude = 20.0f;
			waveSrc.PhaseOffset = 1.5f;
			waveSrc.AngularFrequency = 0.125f;
			waveSrc.Timer = 0.0f;
			waveSrc.Update = UpdateWaveSource;
		}*/

		for (auto& wall : Walls_) {
			auto& waveTex{ wall->WaveTexture };
			waveTex->Begin(dx12Context_);
		}

		decltype(WaveSources_)::Iterator it{ WaveSources_ };
		for (int i = 0; i < 1; ++i) {
			for (it.Begin(); !it.End(); it.Next()) {
				auto& waveSrc{ *it };
				if (waveSrc.Update(waveSrc)) {
					Walls_[waveSrc.WallID]->WaveTexture->Invoke(
						waveSrc.Position,
						waveSrc.Amplitude *
						std::sin(waveSrc.AngularFrequency * waveSrc.Timer + waveSrc.PhaseOffset)
					);
				}
				else {
					WaveSources_.Delete(it);
				}
			}

			for (auto& wall : Walls_) {
				auto& waveTex{ wall->WaveTexture };
				waveTex->Propagate(0.125f);
			}
		}

		for (auto& wall : Walls_) {
			auto& waveTex{ wall->WaveTexture };
			waveTex->End(dx12Context_);
		}
	}

	void Battlefield::RenderGround(
		Lumina::DX12::GraphicsDevice const& device_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::DX12::DescriptorHeap const& localHeap_WorldToNDC_,
		Lumina::DX12::DescriptorTable const& texSRVTable_
	) {
		Ground_->Render(
			device_,
			cmdList_,
			localHeap_WorldToNDC_,
			texSRVTable_,
			VisualBorders_
		);

		/*ImGui::Begin("Debug");
		ImGui::Image(Walls_[0]->WaveTexture->SRVTable().GPUHandle(0U).ptr, { 720.0f, 120.0f });
		ImGui::Image(Walls_[1]->WaveTexture->SRVTable().GPUHandle(0U).ptr, { 720.0f, 120.0f });
		ImGui::Image(Walls_[2]->WaveTexture->SRVTable().GPUHandle(0U).ptr, { 720.0f, 120.0f });
		ImGui::Image(Walls_[3]->WaveTexture->SRVTable().GPUHandle(0U).ptr, { 720.0f, 120.0f });
		ImGui::Image(GlobalTable_Target_.GPUHandle(1U).ptr, { 960.0f, 540.0f });
		ImGui::End();*/
	}

	void Battlefield::RenderWalls(
		Lumina::DX12::GraphicsDevice const& device_,
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::DX12::DescriptorHeap const& localHeap_WorldToNDC_,
		Lumina::DX12::DescriptorHeap const& localHeap_UAV_,
		D3D12_GPU_DESCRIPTOR_HANDLE globalSRV_depthTexture_,
		float timer_
	) {
		device_->CopyDescriptorsSimple(
			1U,
			GlobalTable_WorldToNDC_.CPUHandle(0U),
			localHeap_WorldToNDC_.CPUHandle(0U),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);
		device_->CopyDescriptorsSimple(
			1U,
			GlobalTable_Target_.CPUHandle(0U),
			localHeap_UAV_.CPUHandle(0U),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		);

		cmdList_->OMSetRenderTargets(0U, nullptr, false, nullptr);
		cmdList_->RSSetViewports(1U, &Viewport_);

		cmdList_->SetGraphicsRootSignature(GraphicsRS_Walls_.Get());
		cmdList_->SetPipelineState(GraphicsPSO_Walls_.Get());
		cmdList_->SetGraphicsRootDescriptorTable(0U, GlobalTable_Target_.GPUHandle(0U));
		cmdList_->SetGraphicsRootDescriptorTable(2U, GlobalTable_WorldToNDC_.GPUHandle(0U));
		cmdList_->SetGraphicsRootDescriptorTable(4U, globalSRV_depthTexture_);
		cmdList_->SetGraphicsRoot32BitConstants(5U, 1U, &timer_, 0U);

		cmdList_->IASetVertexBuffers(0U, 1U, &Plane_.VBV);
		cmdList_->IASetIndexBuffer(&Plane_.IBV);
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (auto const& wall : Walls_) {
			cmdList_->SetGraphicsRootConstantBufferView(1U, wall->UB_WorldMatrix->GetGPUVirtualAddress());
			cmdList_->SetGraphicsRootDescriptorTable(3U, wall->WaveTexture->SRVTable().GPUHandle(0U));
			cmdList_->DrawIndexedInstanced(6U, 1U, 0U, 0U, 0U);
		}
	}

	void Battlefield::Initialize(Lumina::DX12::Context const& dx12Context_) {
		auto const& device{ dx12Context_.Device() };
		auto const& globalHeap{ dx12Context_.GlobalDescriptorHeap() };

		Ground_.reset(new Lumina::Terrain{});
		Ground_->Initialize(dx12Context_);

		Plane_.UB_Vertices.Initialize(device, sizeof(PlaneVertex) * 4LLU);
		Plane_.VBV = Lumina::DX12::VBV::Create<PlaneVertex>(Plane_.UB_Vertices);
		PlaneVertex const vertices[4]{
			{ .Position{ -1.0f, 1.0f, 0.0f, 1.0f }, .TexCoord{ 0.0f, 0.0f } },
			{ .Position{ 1.0f, 1.0f, 0.0f, 1.0f }, .TexCoord{ 1.0f, 0.0f } },
			{ .Position{ -1.0f, -1.0f, 0.0f, 1.0f }, .TexCoord{ 0.0f, 1.0f } },
			{ .Position{ 1.0f, -1.0f, 0.0f, 1.0f }, .TexCoord{ 1.0f, 1.0f } },
		};
		Plane_.UB_Vertices.Store(vertices, sizeof(PlaneVertex) * 4LLU, 0LLU);

		Plane_.UB_Indices.Initialize(device, sizeof(uint32_t) * 6LLU);
		Plane_.IBV = Lumina::DX12::IBV::Create(Plane_.UB_Indices);
		uint32_t const indices[6]{ 0U, 1U, 2U, 1U, 3U, 2U };
		Plane_.UB_Indices.Store(indices, sizeof(uint32_t) * 6LLU, 0LLU);

		for (auto& wall : Walls_) {
			wall.reset(new Wall{});

			wall->WorldMatrix = Lumina::Mat4::SRT({ 60.0f, 10.0f, 1.0f }, {}, { 0.0f, 10.0f, 0.0f });

			wall->WaveTexture.reset(new Lumina::Wave2D{});
			wall->WaveTexture->Initialize(dx12Context_, 180U, 30U);
		}

		Walls_[0]->WorldMatrix[3][2] = -60.0f;
		Walls_[1]->WorldMatrix[3][2] = 60.0f;
		Walls_[2]->WorldMatrix *= Lumina::Mat4::SRT(
			{ 1.0f, 1.0f, 1.0f },
			{ 0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f },
			{ -60.0f, 0.0f, 0.0f }
		);
		Walls_[3]->WorldMatrix *= Lumina::Mat4::SRT(
			{ 1.0f, 1.0f, 1.0f },
			{ 0.0f, std::numbers::pi_v<float> * 0.5f, 0.0f },
			{ 60.0f, 0.0f, 0.0f }
		);

		for (auto& wall : Walls_) {
			wall->WorldToLocal = wall->WorldMatrix.Inv();

			wall->UB_WorldMatrix.Initialize(device, 256LLU);
			wall->UB_WorldMatrix.Store(&wall->WorldMatrix, sizeof(Lumina::Mat4), 0LLU);
		}

		WaveSources_.Initialize(256U);

		auto config_Walls{ Lumina::Utils::LoadFromFile<nlohmann::json>("Assets/Configs/Wall.json") };
		GraphicsRS_Walls_.Initialize(
			device,
			Lumina::DX12::LoadRootSignatureSetup(config_Walls.at("RS"))
		);

		dx12Context_.Compile(VS_Walls_, L"Assets/Shaders/Wall.VS.hlsl", L"vs_6_6", L"main", "Wall.VS");
		dx12Context_.Compile(PS_Walls_, L"Assets/Shaders/Wall.PS.hlsl", L"ps_6_6", L"main", "Wall.PS");

		Lumina::DX12::GraphicsPSO::InputLayout inputLayout{};
		inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);
		inputLayout.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
		GraphicsPSO_Walls_.Initialize(
			device,
			GraphicsRS_Walls_,
			VS_Walls_,
			PS_Walls_,
			Lumina::DX12::BlendState{},
			Lumina::DX12::RasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
			},
			Lumina::DX12::DepthStencilState{
				.DepthEnable{ false },
				.StencilEnable{ false },
			},
			inputLayout,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			{ DXGI_FORMAT_UNKNOWN },
			DXGI_FORMAT_UNKNOWN
		);

		GlobalTable_Target_ = globalHeap.Allocate(2U);

		Viewport_ = D3D12_VIEWPORT{
			.TopLeftX{ 0.0f },
			.TopLeftY{ 0.0f },
			.Width{ 1280.0f },
			.Height{ 720.0f },
			.MinDepth{ 0.0f },
			.MaxDepth{ 1.0f },
		};
		
		GlobalTable_WorldToNDC_ = globalHeap.Allocate(1U);
	}
}