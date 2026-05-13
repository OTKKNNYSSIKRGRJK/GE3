export module Lumina.Grayscale;

import <array>;

import Lumina.Math;
import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.Utils.Data;
import Lumina.Fullscreen;

namespace Lumina {
	export class Grayscale {
	public:
		void SetPipeline(
			DX12::CommandList const& cmdList_,
			D3D12_GPU_DESCRIPTOR_HANDLE srv_OffscreenTexture_
		) const {
			cmdList_->SetGraphicsRootSignature(RS_.Get());
			cmdList_->SetGraphicsRootDescriptorTable(0U, srv_OffscreenTexture_);
			cmdList_->SetPipelineState(PSO_.Get());
		}

		void Initialize(
			DX12::Context const& dxContext_,
			DX12::GraphicsDevice const& device_,
			Fullscreen const& fullscreen_
		) {
			auto settings{ Utils::LoadFromFile<nlohmann::json>("Grayscale.json", "Assets/CG5") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device_, rsSetup, "Grayscale RS");

			dxContext_.Compile(
				PS_,
				L"Assets/CG5/Grayscale.PS.hlsl",
				L"ps_6_6",
				L"main",
				"Grayscale.PS"
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
			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
			};
			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ false },
			};
			DX12::GraphicsPipelineState::InputLayout inputLayout{};
			std::vector<DXGI_FORMAT> rtvFormats{
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			};
			graphicsPSOSetup <<
				RS_ <<
				fullscreen_.VertexShader() <<
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
				"Grayscale"
			);
		}

	private:
		DX12::RootSignature RS_;
		DX12::Shader PS_;
		DX12::GraphicsPSO PSO_;
	};
}