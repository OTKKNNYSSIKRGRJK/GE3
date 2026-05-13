export module Lumina.GradientMapping;

import <array>;

import Lumina.Math;
import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.Utils.Data;
import Lumina.Fullscreen;

namespace Lumina {
	export class GradientMapping {
	public:
		auto Gradients() noexcept -> std::array<Float4, 256U>& { return Gradients_; }

		void Update() {
			UB_Gradients_.Store(Gradients_.data(), sizeof(Float4) * Gradients_.size(), 0LLU);
		}

		void SetPipeline(
			DX12::CommandList const& cmdList_,
			D3D12_GPU_DESCRIPTOR_HANDLE srv_OffscreenTexture_
		) const {
			cmdList_->SetGraphicsRootSignature(RS_.Get());
			cmdList_->SetGraphicsRootDescriptorTable(0U, srv_OffscreenTexture_);
			cmdList_->SetGraphicsRootDescriptorTable(1U, GlobalTable_CBV_.GPUHandle(0U));
			cmdList_->SetPipelineState(PSO_.Get());
		}

		void Initialize(
			DX12::Context const& dxContext_,
			DX12::GraphicsDevice const& device_,
			Fullscreen const& fullscreen_
		) {
			auto settings{ Utils::LoadFromFile<nlohmann::json>("GradientMapping.json", "Assets/CG5") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device_, rsSetup, "GradientMapping RS");

			dxContext_.Compile(
				PS_,
				L"Assets/CG5/GradientMapping.PS.hlsl",
				L"ps_6_6",
				L"main",
				"GradientMapping.PS"
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

			constexpr float inv_255{ 1.0f / 255.0f };
			for (int i{ 0 }; i < 256; ++i) {
				float const val{ i * inv_255 };
				Gradients_[i] = { val, val, val, 1.0f };
			}

			constexpr auto fitToValidConstantBufferSize{
				[] (size_t size_) constexpr {
					return ((size_ + 0xFFLLU) & ~0xFFLLU);
				}
			};
			UB_Gradients_.Initialize(device_,
				fitToValidConstantBufferSize(sizeof(Float4) * Gradients_.size())
			);
			UB_Gradients_.Store(Gradients_.data(), sizeof(Float4) * Gradients_.size(), 0LLU);

			GlobalTable_CBV_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			DX12::CBV::Create(device_, GlobalTable_CBV_.CPUHandle(0U), UB_Gradients_);
		}

	private:
		DX12::RootSignature RS_;
		DX12::Shader PS_;
		DX12::GraphicsPSO PSO_;

		DX12::UploadBuffer UB_Gradients_;
		DX12::DescriptorTable GlobalTable_CBV_;
		std::array<Float4, 256U> Gradients_;
	};
}