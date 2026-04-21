export module Lumina.Skybox;

import <string>;
import <array>;
import <vector>;
import <cstdint>;
import <d3d12.h>;

import Lumina.Math.Numerics;
import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.AssetManager;
import Lumina.Utils.Data;

namespace Lumina {
	export class Skybox {
	public:
		void Render(
			DX12::CommandList const& cmdList_,
			DX12::DescriptorTable const& cbvTable_
		) {
			cmdList_->SetGraphicsRootSignature(RS_.Get());
			cmdList_->SetGraphicsRootDescriptorTable(0U, cbvTable_.GPUHandle(0U));
			cmdList_->SetGraphicsRootDescriptorTable(1U, SRV_Textures_.GPUHandle(0U));
			cmdList_->SetPipelineState(PSO_.Get());
			cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList_->IASetVertexBuffers(0U, 1U, &VBV_);
			cmdList_->IASetIndexBuffer(&IBV_);
			cmdList_->DrawIndexedInstanced(36U, 1U, 0U, 0U, 0U);
		}

		void Initialize(
			DX12::Context const& dxContext_,
			DX12::GraphicsDevice const& device_,
			AssetManager& assetMngr_,
			[[maybe_unused]] std::string_view texFilePath_
		) {
			// right
			
			Vertices_[0].Position = { 1.0f, 1.0f, 1.0f, 1.0f };
			Vertices_[1].Position = { 1.0f, 1.0f, -1.0f, 1.0f };
			Vertices_[2].Position = { 1.0f, -1.0f, 1.0f, 1.0f };
			Vertices_[3].Position = { 1.0f, -1.0f, -1.0f, 1.0f };

			// left

			Vertices_[4].Position = { -1.0f, 1.0f, -1.0f, 1.0f };
			Vertices_[5].Position = { -1.0f, 1.0f, 1.0f, 1.0f };
			Vertices_[6].Position = { -1.0f, -1.0f, -1.0f, 1.0f };
			Vertices_[7].Position = { -1.0f, -1.0f, 1.0f, 1.0f };

			// front

			Vertices_[8].Position = { -1.0f, 1.0f, 1.0f, 1.0f };
			Vertices_[9].Position = { 1.0f, 1.0f, 1.0f, 1.0f };
			Vertices_[10].Position = { -1.0f, -1.0f, 1.0f, 1.0f };
			Vertices_[11].Position = { 1.0f, -1.0f, 1.0f, 1.0f };

			// back

			Vertices_[12].Position = { 1.0f, 1.0f, -1.0f, 1.0f };
			Vertices_[13].Position = { -1.0f, 1.0f, -1.0f, 1.0f };
			Vertices_[14].Position = { 1.0f, -1.0f, -1.0f, 1.0f };
			Vertices_[15].Position = { -1.0f, -1.0f, -1.0f, 1.0f };

			// top

			Vertices_[16].Position = { 1.0f, 1.0f, 1.0f, 1.0f };
			Vertices_[17].Position = { -1.0f, 1.0f, 1.0f, 1.0f };
			Vertices_[18].Position = { 1.0f, 1.0f, -1.0f, 1.0f };
			Vertices_[19].Position = { -1.0f, 1.0f, -1.0f, 1.0f };

			// bottom

			Vertices_[20].Position = { -1.0f, -1.0f, 1.0f, 1.0f };
			Vertices_[21].Position = { 1.0f, -1.0f, 1.0f, 1.0f };
			Vertices_[22].Position = { -1.0f, -1.0f, -1.0f, 1.0f };
			Vertices_[23].Position = { 1.0f, -1.0f, -1.0f, 1.0f };

			Indices_ = { 0, 1, 2, 2, 1, 3, };
			for (int i = 1; i < 6; ++i) {
				Indices_[i * 6 + 0] = Indices_[0] + i * 4;
				Indices_[i * 6 + 1] = Indices_[1] + i * 4;
				Indices_[i * 6 + 2] = Indices_[2] + i * 4;
				Indices_[i * 6 + 3] = Indices_[3] + i * 4;
				Indices_[i * 6 + 4] = Indices_[4] + i * 4;
				Indices_[i * 6 + 5] = Indices_[5] + i * 4;
			}

			VertexBuffer_.Initialize(device_, sizeof(Vertex) * 24);
			IndexBuffer_.Initialize(device_, sizeof(uint32_t) * 36);

			VertexBuffer_.Store(Vertices_.data(), sizeof(Vertex) * 24, 0LLU);
			IndexBuffer_.Store(Indices_.data(), sizeof(uint32_t) * 36, 0LLU);

			VBV_ = DX12::VBV::Create<Vertex>(VertexBuffer_);
			IBV_ = DX12::IBV::Create(IndexBuffer_);

			SRV_Textures_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			std::vector<uint32_t> texIDs{};
			assetMngr_.Graphics().LoadImageTextures(
				texIDs,
				{
					{ "Skybox", texFilePath_.data() },
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

			auto settings{ Utils::LoadFromFile<nlohmann::json>("Skybox.json", "Assets/Configs") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device_, rsSetup, "Skybox RS");

			dxContext_.Compile(
				VS_,
				L"Assets/CG4/Skybox.VS.hlsl",
				L"vs_6_6",
				L"main",
				"Skybox.VS"
			);
			dxContext_.Compile(
				PS_,
				L"Assets/CG4/Skybox.PS.hlsl",
				L"ps_6_6",
				L"main",
				"Skybox.PS"
			);
			DX12::GraphicsPipelineState::Setup graphicsPSOSetup{};
			DX12::BlendState blendState{ .IndependentBlendEnable{ true }, };
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ONE },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
			blendState.RenderTarget[1] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_INV_SRC_ALPHA },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ONE },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_BACK },
			};
			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ true },
				.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ZERO },
				.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
			};
			DX12::GraphicsPipelineState::InputLayout inputLayout{};
			inputLayout.Append("POSITION", 0U, DXGI_FORMAT_R32G32B32_FLOAT);
			std::vector<DXGI_FORMAT> rtvFormats{
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				DXGI_FORMAT_R8G8B8A8_UNORM,
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
		}

	private:
		struct Vertex {
			Float4 Position;
		};

		std::array<Vertex, 24> Vertices_;
		std::array<uint32_t, 36> Indices_;

		DX12::RootSignature RS_;
		DX12::Shader VS_;
		DX12::Shader PS_;
		DX12::GraphicsPSO PSO_;

		DX12::DescriptorTable SRV_Textures_;

		DX12::UploadBuffer VertexBuffer_;
		DX12::UploadBuffer IndexBuffer_;
		D3D12_VERTEX_BUFFER_VIEW VBV_;
		D3D12_INDEX_BUFFER_VIEW IBV_;
	};
}