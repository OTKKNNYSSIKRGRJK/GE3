export module Lumina.SimpleFX3;

import <random>;
import <vector>;
import <cstdint>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.AssetManager;
import Lumina.Utils.Data;
import Lumina.Utils.Color;

namespace Lumina {
	export class SimpleFX3 {
	public:
		struct CylinderProperties {
			uint32_t NUM_Division;
			float Height;
			float Radius_Top;
			float Radius_Bottom;
		};

		struct ShaderConstant {
			float WorldToProjective[4][4];
			float Time;
			Float4 Color0;
			Float4 Color1;
		};

	public:
		CylinderProperties ResetCylinder(CylinderProperties const& props_) {
			CylinderProperties props{};
			{
				props.NUM_Division = std::clamp<uint32_t>(
					props_.NUM_Division,
					NUM_Division_MIN,
					NUM_Division_MAX
				);
				props.Height = std::clamp<float>(
					props_.Height,
					0.001f,
					10.0f
				);
				props.Radius_Top = std::clamp<float>(
					props_.Radius_Top,
					0.001f,
					10.0f
				);
				props.Radius_Bottom = std::clamp<float>(
					props_.Radius_Bottom,
					0.001f,
					10.0f
				);
			}

			float const inv_NUM_DIV{ 1.0f / static_cast<float>(props.NUM_Division) };
			float const radianPerDivision{ std::numbers::pi_v < float> * 2.0f * inv_NUM_DIV };

			std::vector<std::pair<float, float>> lookup_SINCOSs;
			{
				lookup_SINCOSs.resize(props.NUM_Division + 1U);

				lookup_SINCOSs[0] = { 0.0f, 1.0f };
				lookup_SINCOSs[props.NUM_Division] = { 0.0f, 1.0f };
				if ((props.NUM_Division & 1U) == 0U) {
					lookup_SINCOSs[props.NUM_Division >> 1U] = { 0.0f, -1.0f };
				}
				
				for (uint32_t idx{ 1U }; idx < ((props.NUM_Division + 1U) >> 1U); ++idx) {
					lookup_SINCOSs[idx] = {
						std::sin(idx * radianPerDivision),
						std::cos(idx * radianPerDivision)
					};
					lookup_SINCOSs[props.NUM_Division - idx] = {
						-lookup_SINCOSs[idx].first,
						lookup_SINCOSs[idx].second,
					};
				}
			}

			Vertices_.clear();
			Vertices_.resize(props.NUM_Division * 4U);
			Indices_.clear();
			Indices_.resize(props.NUM_Division * 6U);
			for (uint32_t idx_DIV{ 0U }; idx_DIV < props.NUM_Division; ++idx_DIV) {
				auto const& sinCos_CUR{ lookup_SINCOSs[idx_DIV] };
				auto const& sinCos_NEXT{ lookup_SINCOSs[idx_DIV + 1U] };

				float const texCoordU_CUR{ static_cast<float>(idx_DIV) * inv_NUM_DIV };
				float const texCoordU_NEXT{ static_cast<float>(idx_DIV + 1U) * inv_NUM_DIV };

				Vertices_[idx_DIV * 4U + 0U] = {
					Float4{
						-sinCos_CUR.first * props.Radius_Top,
						props.Height,
						sinCos_CUR.second * props.Radius_Top,
						1.0f
					},
					Float2{ texCoordU_CUR, 1.0f },
					Float3{ -sinCos_CUR.first, 0.0f, sinCos_CUR.second }
				};
				Vertices_[idx_DIV * 4U + 1U] = {
					Float4{
						-sinCos_NEXT.first * props.Radius_Top,
						props.Height,
						sinCos_NEXT.second * props.Radius_Top,
						1.0f
					},
					Float2{ texCoordU_NEXT, 1.0f },
					Float3{ -sinCos_NEXT.first, 0.0f, sinCos_NEXT.second }
				};
				Vertices_[idx_DIV * 4U + 2U] = {
					Float4{
						-sinCos_CUR.first * props.Radius_Bottom,
						0.0f,
						sinCos_CUR.second * props.Radius_Bottom,
						1.0f
					},
					Float2{ texCoordU_CUR, 0.0f },
					Float3{ -sinCos_CUR.first, 0.0f, sinCos_CUR.second }
				};
				Vertices_[idx_DIV * 4U + 3U] = {
					Float4{
						-sinCos_NEXT.first * props.Radius_Bottom,
						0.0f,
						sinCos_NEXT.second * props.Radius_Bottom,
						1.0f
					},
					Float2{ texCoordU_NEXT, 0.0f },
					Float3{ -sinCos_NEXT.first, 0.0f, sinCos_NEXT.second }
				};

				Indices_[idx_DIV * 6U + 0U] = idx_DIV * 4U + 0U;
				Indices_[idx_DIV * 6U + 1U] = idx_DIV * 4U + 1U;
				Indices_[idx_DIV * 6U + 2U] = idx_DIV * 4U + 2U;
				Indices_[idx_DIV * 6U + 3U] = idx_DIV * 4U + 1U;
				Indices_[idx_DIV * 6U + 4U] = idx_DIV * 4U + 3U;
				Indices_[idx_DIV * 6U + 5U] = idx_DIV * 4U + 2U;
			}

			VertexBuffer_.Store(Vertices_.data(), sizeof(Vertex) * Vertices_.size(), 0LLU);
			IndexBuffer_.Store(Indices_.data(), sizeof(uint32_t) * Indices_.size(), 0LLU);

			CylinderProperties_ = props;
			return props;
		}

	private:
		int BatchedNum_ = 0;
	public:
		void Batch(
			Mat4 const& world_
		) {
			UB_Worlds_.Store(&world_, sizeof(Mat4), sizeof(Mat4) * BatchedNum_);
			++BatchedNum_;
		}

		void ClearBatch() { BatchedNum_ = 0; }

		void Render(
			DX12::CommandList const& cmdList_,
			Mat4 const& worldToProjective_
		) {
			if (!BatchedNum_) { return; }

			/*auto& rndGen{ reinterpret_cast<std::mt19937&>(Random::Generator()) };
			static std::uniform_real_distribution<float> distScale{ -0.0625f, 0.0625f };
			for (uint32_t idx_DIV{ 0U }; idx_DIV < CylinderProperties_.NUM_Division; ++idx_DIV) {
				Vertices_[idx_DIV * 4U + 0U].TexCoord.y = 0.0f + distScale(rndGen);
				Vertices_[idx_DIV * 4U + 2U].TexCoord.y = 1.0f + distScale(rndGen);
			}
			for (uint32_t idx_DIV{ 0U }; idx_DIV < CylinderProperties_.NUM_Division - 1; ++idx_DIV) {
				Vertices_[idx_DIV * 4U + 1U].TexCoord.y = Vertices_[idx_DIV * 4U + 4U].TexCoord.y;
				Vertices_[idx_DIV * 4U + 3U].TexCoord.y = Vertices_[idx_DIV * 4U + 6U].TexCoord.y;
			}
			Vertices_[(CylinderProperties_.NUM_Division - 1) * 4U + 1U].TexCoord.y = Vertices_[0U].TexCoord.y;
			Vertices_[(CylinderProperties_.NUM_Division - 1) * 4U + 3U].TexCoord.y = Vertices_[2U].TexCoord.y;
			VertexBuffer_.Store(Vertices_.data(), sizeof(Vertex) * Vertices_.size(), 0LLU);*/

			Time_ += 0.0166667f;

			UB_Constants_.Store(
				&worldToProjective_,
				sizeof(Mat4),
				0LLU
			);

			static float h0{};
			static float h0T = 0.0f;
			h0 = 30.0f + std::sin(h0T) * 60.0f;
			h0T += 0.05f;
			auto rgb0 = Utils::Color::Convert(
				Utils::Color::HSV{ h0, 0.75f, 0.75f }
			);
			static float h1{};
			static float h1T = 0.0f;
			h1 = 210.0f + std::sin(h1T) * 30.0f;
			h1T += 0.03f;
			auto rgb1 = Utils::Color::Convert(
				Utils::Color::HSV{ h1, 0.75f, 1.0f }
			);
			Float4 color0{ rgb0.R, rgb0.G, rgb0.B, 1.0f };
			Float4 color1{ rgb1.R, rgb1.G, rgb1.B, 1.0f };
			UB_Constants_.Store(
				&color0,
				sizeof(Float4),
				sizeof(Mat4)
			);
			UB_Constants_.Store(
				&color1,
				sizeof(Float4),
				sizeof(Mat4) + sizeof(Float4)
			);
			UB_Constants_.Store(
				&Time_,
				sizeof(float),
				sizeof(Mat4) + sizeof(Float4) * 2
			);

			cmdList_->SetGraphicsRootSignature(RS_.Get());
			cmdList_->SetGraphicsRootDescriptorTable(0U, CBV_Constants_.GPUHandle(0U));
			cmdList_->SetGraphicsRootDescriptorTable(1U, SRV_Textures_.GPUHandle(0U));
			cmdList_->SetPipelineState(PSO_.Get());
			cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList_->IASetVertexBuffers(0U, 1U, &VBV_);
			cmdList_->IASetIndexBuffer(&IBV_);
			cmdList_->DrawIndexedInstanced(
				CylinderProperties_.NUM_Division * 6U,
				BatchedNum_,
				0U, 0U, 0U
			);
		}

		void Initialize(
			DX12::Context const& dxContext_,
			DX12::GraphicsDevice const& device_,
			AssetManager& assetMngr_
		) {
			SRV_Textures_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "FX3", "Assets/GFX/gradationLine.png" },
				}
			);
			for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(texIDs.size()); ++idx) {
				device_->CopyDescriptorsSimple(
					1U,
					SRV_Textures_.CPUHandle(idx),
					assetMngr_.Graphics().CPUHandle(texIDs.at(idx)),
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}

			auto settings{ Utils::LoadFromFile<nlohmann::json>("Cylinder.json", "Assets/Configs") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device_, rsSetup, "Cylinder RS");

			dxContext_.Compile(
				VS_,
				L"Assets/CG4/Cylinder.VS.hlsl",
				L"vs_6_6",
				L"main",
				"SimpleFX3.VS"
			);
			dxContext_.Compile(
				PS_,
				L"Assets/CG4/Cylinder.PS.hlsl",
				L"ps_6_6",
				L"main",
				"SimpleFX3.PS"
			);
			DX12::GraphicsPipelineState::Setup graphicsPSOSetup{};
			DX12::BlendState blendState{ .IndependentBlendEnable{ true }, };
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ONE },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
			};
			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ true },
				.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ZERO },
				.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
			};
			DX12::GraphicsPipelineState::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT);
			inputLayout.Append("TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT);
			inputLayout.Append("NORMAL", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			std::vector<DXGI_FORMAT> rtvFormats{
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			};
			graphicsPSOSetup <<
				RS_ <<
				VS_ <<
				PS_ <<
				blendState <<
				rasterizerState <<
				depthStencilState <<
				inputLayout <<
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE <<
				rtvFormats <<
				DX12::GraphicsPipelineState::DefaultDSVFormat;
			PSO_.Initialize(
				device_,
				graphicsPSOSetup,
				"CG3Eval2::GraphicsPSO"
			);

			VertexBuffer_.Initialize(device_, sizeof(Vertex) * NUM_Division_MAX * 4U);
			IndexBuffer_.Initialize(device_, sizeof(uint32_t) * NUM_Division_MAX * 6U);

			VBV_ = DX12::VBV::Create<Vertex>(VertexBuffer_);
			IBV_ = DX12::IBV::Create(IndexBuffer_);

			UB_Constants_.Initialize(device_, 256LLU);
			CBV_Constants_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			DX12::CBV::Create(device_, CBV_Constants_.CPUHandle(0U), UB_Constants_);

			UB_Worlds_.Initialize(device_, sizeof(Mat4) * 64LLU);
			SRV_Worlds_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			DX12::SRV<Mat4>::Create(device_, SRV_Worlds_.CPUHandle(0U), UB_Worlds_);

			Time_ = 0.0f;
		}

	private:
		struct Vertex {
			Float4 Position;
			Float2 TexCoord;
			Float3 Normal;
		};

		std::vector<Vertex> Vertices_;
		std::vector<uint32_t> Indices_;

		DX12::UploadBuffer VertexBuffer_;
		DX12::UploadBuffer IndexBuffer_;
		D3D12_VERTEX_BUFFER_VIEW VBV_;
		D3D12_INDEX_BUFFER_VIEW IBV_;


		DX12::RootSignature RS_;
		DX12::Shader VS_;
		DX12::Shader PS_;
		DX12::GraphicsPSO PSO_;

		DX12::UploadBuffer UB_Constants_;
		DX12::UploadBuffer UB_Worlds_;
		DX12::DescriptorTable CBV_Constants_;
		DX12::DescriptorTable SRV_Worlds_;
		DX12::DescriptorTable SRV_Textures_;

	public:
		constexpr static uint32_t NUM_Division_MIN{ 3U };
		constexpr static uint32_t NUM_Division_MAX{ 128U };

	private:
		CylinderProperties CylinderProperties_;
		float Time_;
	};
}