export module Lumina.CG5.Random;

import <string_view>;

import Lumina.Math;
import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.Utils.Data;
import Lumina.Fullscreen;

namespace Lumina {
	export class CG5Random {
	public:
		void UpdateConstant(
			void const* constantData_,
			uint64_t sizeInBytes_,
			uint64_t offsetInBytes_ = 0LLU
		) {
			UB_Constants_.Store(constantData_, sizeInBytes_, offsetInBytes_);
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
			Fullscreen const& fullscreen_,
			std::wstring_view filePath_PS_
		) {
			auto settings{ Utils::LoadFromFile<nlohmann::json>("Random.json", "Assets/CG5") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device_, rsSetup, "Random RS");

			dxContext_.Compile(
				PS_,
				filePath_PS_.data(),
				L"ps_6_6",
				L"main",
				"Random.PS"
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
				"RadialBlur"
			);

			constexpr auto fitToValidConstantBufferSize{
				[](size_t size_) constexpr {
					return ((size_ + 0xFFLLU) & ~0xFFLLU);
				}
			};
			UB_Constants_.Initialize(device_, 1024LLU);

			GlobalTable_CBV_ = dxContext_.GlobalDescriptorHeap().Allocate(1U);
			DX12::CBV::Create(device_, GlobalTable_CBV_.CPUHandle(0U), UB_Constants_);
		}

	private:
		DX12::RootSignature RS_;
		DX12::Shader PS_;
		DX12::GraphicsPSO PSO_;

		DX12::UploadBuffer UB_Constants_;
		DX12::DescriptorTable GlobalTable_CBV_;
	};
}