module;

#include<d3d12.h>

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

//#include<External/ImGui/imgui.h>

export module Lumina.ProceduralTerrain;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import Lumina.Utils.Data;

import Lumina.Math.Numerics;
import Lumina.Math.Vector;
import Lumina.Math.PerlinNoise;

import Lumina.Utils.ImGui;

namespace Lumina {
	namespace {
		struct Vertex {
			Float4 Position;
		};

		struct MapSurflet {
			float Elevation;
			Float3 Normal;
		};

		struct Climate {
			float Temperature;
			float Precipitation;
		};

		struct NoiseParam {
			float Frequency;
			float Redist;
			Float3 Offset;
			uint32_t Num_Octaves;
			float Persistance;
		};
	}

	export class Terrain {
	public:
		void UpdateElevation(DX12::CommandQueue& directQueue_);
		void UpdateTemperature(DX12::CommandQueue& directQueue_);
		void UpdatePrecipitation(DX12::CommandQueue& directQueue_);
		void Update(DX12::CommandQueue& directQueue_);

		void Render(
			DX12::CommandList& directList_,
			const DX12::DescriptorHeap& cbvsrvuavHeap_,
			const DX12::DescriptorTable& vpCBVTable_,
			const DX12::DescriptorTable& texSRVTable_
		);

		void Initialize(
			DX12::Context const& dxContext_
		);

		Terrain() = default;
		~Terrain();

	private:
		DX12::CommandAllocator ComputeAllocator_{};
		DX12::CommandList ComputeList_{};
		DX12::CommandAllocator DirectAllocator_{};
		DX12::CommandList DirectList_{};

		DX12::DefaultBuffer MapSurfletBuffer_{};
		DX12::UploadBuffer MapSurfletUpload_{};
		DX12::DefaultBuffer ClimateBuffer_{};
		DX12::UploadBuffer ClimateUpload_{};

		DX12::DescriptorTable CSUTable_{};

		/*DX12::RootSignature NoiseRS_{};
		DX12::Shader NoiseShader_{};
		DX12::ComputePSO NoisePSO_{};*/

		DX12::RootSignature RenderRS_{};
		DX12::Shader RenderVertexShader_{};
		DX12::Shader RenderPixelShader_{};
		DX12::GraphicsPSO RenderPSO_{};

		MapSurflet* MapSurflets_{ nullptr };
		Climate* ClimateData_{ nullptr };
		uint32_t Width_{ 512U };
		uint32_t Height_{ 512U };

		DX12::DefaultBuffer QuadVertexBuffer_{};
		DX12::DefaultBuffer QuadIndexBuffer_{};
		D3D12_VERTEX_BUFFER_VIEW QuadVBV_{};
		D3D12_INDEX_BUFFER_VIEW QuadIBV_{};

		NoiseParam ElevationNoiseParam_{
			.Frequency{ 0.4f },
			.Redist{ 2.0f },
			.Offset{ 0.0f, 0.0f, 0.0f },
			.Num_Octaves{ 8U },
			.Persistance{ 0.5f },
		};
		float Insulation_{ 0.3f };
		int IsFormingTerraces_{ 0 };
		float TerraceFactor_{ 8.0f };

		NoiseParam TemperatureNoiseParam_{
			.Frequency{ 1.0f },
			.Redist{ 1.0f },
			.Offset{ 1.0f, 3.0f, 5.0f },
			.Num_Octaves{ 2U },
			.Persistance{ 0.25f },
		};
		NoiseParam PrecipitationNoiseParam_{
			.Frequency{ 2.0f },
			.Redist{ 1.0f },
			.Offset{ 2.0f, 3.0f, 4.0f },
			.Num_Octaves{ 2U },
			.Persistance{ 0.25f },
		};
	};

	void Terrain::UpdateElevation(DX12::CommandQueue& directQueue_) {
		const Math::PerlinNoise noiseGen{
			ElevationNoiseParam_.Frequency,
			ElevationNoiseParam_.Num_Octaves,
			ElevationNoiseParam_.Persistance,
			ElevationNoiseParam_.Offset
		};

		constexpr float div{ 1.0f / 32.0f };
		const float inv_Width = 1.0f / static_cast<float>(Width_);
		const float inv_Height = 1.0f / static_cast<float>(Height_);
		float inv_TerraceFactor_{ 1.0f / TerraceFactor_ };

		for (uint32_t v = 0; v < Height_; ++v) {
			for (uint32_t u = 0; u < Width_; ++u) {
				const float noise{ noiseGen(u * div, v * div, 0.0f) };

				const float nu = 2.0f * u * inv_Width - 1.0f;
				const float nv = 2.0f * v * inv_Height - 1.0f;
				const float island = (1.0f - nu * nu) * (1.0f - nv * nv);
				const float val = noise * (1.0f - Insulation_) + island * Insulation_;

				auto& surflet{ MapSurflets_[v * Width_ + u] };

				surflet.Elevation = std::pow(val, ElevationNoiseParam_.Redist);
				surflet.Elevation *= 2.0f;
				if (IsFormingTerraces_) {
					surflet.Elevation *= TerraceFactor_;
					surflet.Elevation = std::round(surflet.Elevation);
					surflet.Elevation *= inv_TerraceFactor_;
				}
			}
		}

		for (uint32_t v = 0; v < Height_; ++v) {
			uint32_t v0 = (v == 0) ? (v) : (v - 1);
			uint32_t v1 = (v == Height_ - 1) ? (v) : (v + 1);
			
			for (uint32_t u = 0; u < Width_; ++u) {
				uint32_t u0 = (u == 0) ? (u) : (u - 1);
				uint32_t u1 = (u == Width_ - 1) ? (u) : (u + 1);

				Vec3 df_du{ (u1 - u0) * div, 0.0f, MapSurflets_[v * Width_ + u1].Elevation - MapSurflets_[v * Width_ + u0].Elevation};
				Vec3 df_dv{ 0.0f, (v1 - v0) * div, MapSurflets_[v1 * Width_ + u].Elevation - MapSurflets_[v0 * Width_ + u].Elevation };
				Vec3 unit{ Vec3::Cross(df_du, df_dv) };
				unit = unit.Unit();
				MapSurflets_[v * Width_ + u].Normal = { unit.x, unit.y, unit.z };
			}
		}

		MapSurfletUpload_.Store(MapSurflets_, MapSurfletUpload_.SizeInBytes(), 0U);
		DirectList_->CopyResource(MapSurfletBuffer_.Get(), MapSurfletUpload_.Get());
		directQueue_ << DirectList_;
		directQueue_.CPUWait(directQueue_.ExecuteBatchedCommandLists());
		DirectList_.Reset(DirectAllocator_);
	}

	void Terrain::UpdateTemperature(DX12::CommandQueue& directQueue_) {
		Math::PerlinNoise noiseGen{
			TemperatureNoiseParam_.Frequency,
			TemperatureNoiseParam_.Num_Octaves,
			TemperatureNoiseParam_.Persistance,
			TemperatureNoiseParam_.Offset
		};

		constexpr float div{ 1.0f / 32.0f };
		const float inv_Height = 1.0f / static_cast<float>(Height_);

		for (uint32_t v = 0; v < Height_; ++v) {
			float latFactor{
				(1.0f - std::abs(static_cast<int32_t>(v << 1U) - static_cast<int32_t>(Height_)) * inv_Height)
			};
			for (uint32_t u = 0; u < Width_; ++u) {
				const float noise{ noiseGen(u * div, v * div, 0.0f) };

				float elvFactor{
					1.0f - MapSurflets_[v * Width_ + u].Elevation * 0.5f
				};

				auto& climate{ ClimateData_[v * Width_ + u] };
				climate.Temperature = std::pow(noise, TemperatureNoiseParam_.Redist);
				climate.Temperature =
					climate.Temperature * 0.65f +
					std::pow(latFactor, 1.5f) * 0.35f;
				climate.Temperature *= elvFactor;
			}
		}

		ClimateUpload_.Store(ClimateData_, ClimateUpload_.SizeInBytes(), 0U);
		DirectList_->CopyResource(ClimateBuffer_.Get(), ClimateUpload_.Get());
		directQueue_ << DirectList_;
		directQueue_.CPUWait(directQueue_.ExecuteBatchedCommandLists());
		DirectList_.Reset(DirectAllocator_);
	}

	void Terrain::UpdatePrecipitation(DX12::CommandQueue& directQueue_) {
		Math::PerlinNoise noiseGen{
			PrecipitationNoiseParam_.Frequency,
			PrecipitationNoiseParam_.Num_Octaves,
			PrecipitationNoiseParam_.Persistance,
			PrecipitationNoiseParam_.Offset
		};

		constexpr float div{ 1.0f / 32.0f };

		for (uint32_t v = 0; v < Height_; ++v) {
			for (uint32_t u = 0; u < Width_; ++u) {
				const float noise{ noiseGen(u * div, v * div, 0.0f) };

				auto& climate{ ClimateData_[v * Width_ + u] };
				climate.Precipitation = std::pow(noise, PrecipitationNoiseParam_.Redist);
			}
		}

		ClimateUpload_.Store(ClimateData_, ClimateUpload_.SizeInBytes(), 0U);
		DirectList_->CopyResource(ClimateBuffer_.Get(), ClimateUpload_.Get());
		directQueue_ << DirectList_;
		directQueue_.CPUWait(directQueue_.ExecuteBatchedCommandLists());
		DirectList_.Reset(DirectAllocator_);
	}

	void Terrain::Update(DX12::CommandQueue& directQueue_){
		ImGui::Begin("Procedural Terrain");

		ImGui::SeparatorText("Elevation");
		ImGui::DragFloat("Frequency##TerElv", &ElevationNoiseParam_.Frequency, 0.01f);
		ImGui::DragFloat("Redistribution##TerElv", &ElevationNoiseParam_.Redist, 0.01f);
		ImGui::DragFloat3("Offset##TerElv", reinterpret_cast<float*>(&ElevationNoiseParam_.Offset), 0.01f);
		ImGui::DragInt("Octaves##TerElv", reinterpret_cast<int*>(&ElevationNoiseParam_.Num_Octaves), 0.1f, 1, 32);
		ImGui::DragFloat("Persistance##TerElv", &ElevationNoiseParam_.Persistance, 0.01f, 0.0f, 0.875f);
		ImGui::DragFloat("Insulation##TerElv", &Insulation_, 0.01f, 0.0f, 1.0f);
		ImGui::Checkbox("Is Forming Terraces##TerElv", reinterpret_cast<bool*>(&IsFormingTerraces_));
		ImGui::DragFloat("Terrace Factor##TerElv", &TerraceFactor_, 0.01f, 1.0f, 32.0f);
		if (ImGui::Button("Update##TerElv")) {
			UpdateElevation(directQueue_);
		}

		ImGui::SeparatorText("Temperature");
		ImGui::DragFloat("Frequency##TerTmp", &TemperatureNoiseParam_.Frequency, 0.01f);
		ImGui::DragFloat("Redistribution##TerTmp", &TemperatureNoiseParam_.Redist, 0.01f);
		ImGui::DragFloat3("Offset##TerTmp", reinterpret_cast<float*>(&TemperatureNoiseParam_.Offset), 0.01f);
		ImGui::DragInt("Octaves##TerTmp", reinterpret_cast<int*>(&TemperatureNoiseParam_.Num_Octaves), 0.1f, 1, 32);
		ImGui::DragFloat("Persistance##TerTmp", &TemperatureNoiseParam_.Persistance, 0.01f, 0.0f, 0.875f);
		if (ImGui::Button("Update##TerTmp")) {
			UpdateTemperature(directQueue_);
		}

		ImGui::SeparatorText("Precipitation");
		ImGui::DragFloat("Frequency##TerPcp", &PrecipitationNoiseParam_.Frequency, 0.01f);
		ImGui::DragFloat("Redistribution##TerPcp", &PrecipitationNoiseParam_.Redist, 0.01f);
		ImGui::DragFloat3("Offset##TerPcp", reinterpret_cast<float*>(&PrecipitationNoiseParam_.Offset), 0.01f);
		ImGui::DragInt("Octaves##TerPcp", reinterpret_cast<int*>(&PrecipitationNoiseParam_.Num_Octaves), 0.1f, 1, 32);
		ImGui::DragFloat("Persistance##TerPcp", &PrecipitationNoiseParam_.Persistance, 0.01f, 0.0f, 0.875f);
		if (ImGui::Button("Update##TerPcp")) {
			UpdatePrecipitation(directQueue_);
		}

		ImGui::End();
	}

	void Terrain::Render(
		DX12::CommandList& directList_,
		const DX12::DescriptorHeap& cbvsrvuavHeap_,
		const DX12::DescriptorTable& vpCBVTable_,
		const DX12::DescriptorTable& texSRVTable_
	) {
		ID3D12DescriptorHeap* descriptorHeaps[]{ cbvsrvuavHeap_.Get() };
		directList_->SetDescriptorHeaps(1U, descriptorHeaps);
		directList_->SetPipelineState(RenderPSO_.Get());
		directList_->SetGraphicsRootSignature(RenderRS_.Get());
		directList_->SetGraphicsRootDescriptorTable(0U, CSUTable_.GPUHandle(0U));
		directList_->SetGraphicsRootDescriptorTable(1U, vpCBVTable_.GPUHandle(0U));
		directList_->SetGraphicsRootDescriptorTable(2U, texSRVTable_.GPUHandle(0U));
		directList_->IASetVertexBuffers(0U, 1U, &QuadVBV_);
		directList_->IASetIndexBuffer(&QuadIBV_);
		directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		directList_->DrawIndexedInstanced(6U, Width_ * Height_, 0U, 0U, 0U);
	}

	void Terrain::Initialize(
		DX12::Context const& dxContext_
	) {
		auto const& device{ dxContext_.Device() };
		auto& directQueue{ dxContext_.DirectQueue() };
		auto const& gpuDH{ dxContext_.GlobalDescriptorHeap() };

		ComputeAllocator_.Initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE, "ComputeCmdAllocator@Terrain");
		ComputeList_.Initialize(device, ComputeAllocator_, "ComputeCmdList@Terrain");
		DirectAllocator_.Initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT, "GraphicsCmdAllocator@Terrain");
		DirectList_.Initialize(device, DirectAllocator_, "GraphicsCmdList@Terrain");

		MapSurflets_ = new MapSurflet[Width_ * Height_];
		MapSurfletBuffer_.Initialize(device, sizeof(MapSurflet) * Width_ * Height_, "MapSurflet");
		MapSurfletUpload_.Initialize(device, MapSurfletBuffer_.SizeInBytes(), "MapSurfletUpload");

		ClimateData_ = new Climate[Width_ * Height_];
		ClimateBuffer_.Initialize(device, sizeof(Climate) * Width_ * Height_, "ClimateMap");
		ClimateUpload_.Initialize(device, ClimateBuffer_.SizeInBytes(), "ClimateMapUpload");

		CSUTable_ = gpuDH.Allocate(2U);
		DX12::SRV<MapSurflet>::Create(device, CSUTable_.CPUHandle(0U), MapSurfletBuffer_);
		DX12::SRV<Climate>::Create(device, CSUTable_.CPUHandle(1U), ClimateBuffer_);

		//----	------	------	------	------	----//

		NLohmannJSON config{ Utils::LoadFromFile<NLohmannJSON>("Assets/Configs/TerrainConfig.json") };
		auto&& renderRSSetup{ DX12::LoadRootSignatureSetup(config.at("Terrain Render RS")) };
		RenderRS_.Initialize(device, renderRSSetup, "RenderRS@Terrain");
		dxContext_.Compile(
			RenderVertexShader_,
			L"Assets/ShaderTest/TerrainVS.hlsl",
			L"vs_6_6",
			L"main",
			"RenderVS@Terrain"
		);
		dxContext_.Compile(
			RenderPixelShader_,
			L"Assets/ShaderTest/TerrainPS.hlsl",
			L"ps_6_6",
			L"main",
			"RenderPS@Terrain"
		);
		DX12::BlendState blendState{};
		{
			blendState.AlphaToCoverageEnable = false;
			blendState.IndependentBlendEnable = false;
			blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
				.DestBlendAlpha{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
		}
		auto&& rasterizerState{ DX12::LoadRasterizerState(config.at("Terrain Render PSO")) };
		DX12::DepthStencilState depthStencilState{
			.DepthEnable{ true },
			.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
			.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
		};
		auto&& inputLayout{ DX12::LoadInputLayout(config.at("Terrain Render PSO")) };
		RenderPSO_.Initialize(
			device,
			RenderRS_,
			RenderVertexShader_,
			RenderPixelShader_,
			blendState,
			rasterizerState,
			depthStencilState,
			inputLayout,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			DX12::GraphicsPSO::DefaultRTVFormats,
			DX12::GraphicsPSO::DefaultDSVFormat,
			"RenderPSO@Terrain"
		);

		//----	------	------	------	------	----//

		QuadVertexBuffer_.Initialize(device, sizeof(Vertex) * 4U, "VB@Terrain");
		QuadVBV_ = DX12::VBV<Vertex>{ QuadVertexBuffer_ };
		Vertex quadVerts[4]{
			{ .Position{ -0.5f, 0.5f, 0.0f, 1.0f } },
			{ .Position{ 0.5f, 0.5f, 0.0f, 1.0f } },
			{ .Position{ -0.5f, -0.5f, 0.0f, 1.0f } },
			{ .Position{ 0.5f, -0.5f, 0.0f, 1.0f } },
		};
		DX12::UploadBuffer vbTmp{};
		vbTmp.Initialize(device, sizeof(Vertex) * 4U);
		vbTmp.Store(quadVerts, QuadVertexBuffer_.SizeInBytes(), 0LLU);

		QuadIndexBuffer_.Initialize(device, sizeof(uint32_t) * 6U, "IB@Terrain");
		QuadIBV_ = DX12::IBV{ QuadIndexBuffer_ };
		uint32_t quadIndices[6]{ 0U, 1U, 2U, 1U, 3U, 2U, };
		DX12::UploadBuffer ibTmp{};
		ibTmp.Initialize(device, sizeof(uint32_t) * 6U);
		ibTmp.Store(quadIndices, QuadIndexBuffer_.SizeInBytes(), 0LLU);

		DirectList_->CopyResource(QuadVertexBuffer_.Get(), vbTmp.Get());
		DirectList_->CopyResource(QuadIndexBuffer_.Get(), ibTmp.Get());
		directQueue << DirectList_;
		directQueue.CPUWait(directQueue.ExecuteBatchedCommandLists());
		DirectList_.Reset(DirectAllocator_);

		UpdateElevation(directQueue);
	}

	Terrain::~Terrain() {
		if (MapSurflets_ != nullptr) {
			delete[] MapSurflets_;
			MapSurflets_ = nullptr;
		}
		if (ClimateData_ != nullptr) {
			delete[] ClimateData_;
			ClimateData_ = nullptr;
		}
	}
}