export module Lumina.Filtering;

import <array>;
import <string_view>;

import Lumina.Math;
import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;
import Lumina.Utils.Data;
import Lumina.Fullscreen;

namespace Lumina {
	export class Filtering {
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
			cmdList_->SetGraphicsRootDescriptorTable(2U, GlobalTable_CBV_.GPUHandle(2U));
			cmdList_->SetPipelineState(PSO_.Get());
		}

		template<typename..._KernelDistributions>
		void Initialize(
			DX12::Context const& dxContext_,
			DX12::GraphicsDevice const& device_,
			Fullscreen const& fullscreen_,
			std::wstring_view filePath_PS_,
			uint32_t kernelWidth_,
			uint32_t kernelHeight_,
			_KernelDistributions...kernelDists_
		) {
			auto settings{ Utils::LoadFromFile<nlohmann::json>("Filtering.json", "Assets/CG5") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device_, rsSetup, "Filtering RS");

			dxContext_.Compile(
				PS_,
				filePath_PS_.data(),
				L"ps_6_6",
				L"main",
				"Convolution.PS"
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
				"Filtering"
			);

			for (int32_t y{ 0 }; y < static_cast<int32_t>(kernelWidth_); ++y) {
				for (int32_t x{ 0 }; x < static_cast<int32_t>(kernelHeight_); ++x) {
					int32_t const idx{ y * static_cast<int32_t>(kernelWidth_) + x };
					Offsets_[idx] = {
						static_cast<float>(x) - (static_cast<float>(kernelWidth_) - 1.0f) * 0.5f,
						static_cast<float>(y) - (static_cast<float>(kernelHeight_) - 1.0f) * 0.5f
					};
				}
			}

			constexpr auto fitToValidConstantBufferSize{
				[](size_t size_) constexpr {
					return ((size_ + 0xFFLLU) & ~0xFFLLU);
				}
			};
			UB_Constants_.Initialize(device_, 1024LLU);

			UB_Offsets_.Initialize(device_,
				fitToValidConstantBufferSize(
					sizeof(decltype(Offsets_)::value_type) * Offsets_.size()
				)
			);
			UB_Offsets_.Store(
				Offsets_.data(),
				sizeof(decltype(Offsets_)::value_type) * Offsets_.size(),
				0LLU
			);

			std::array<float, 9 * 9> kernel{};
			for (auto& ub_Kernel : UBs_Kernel_) {
				ub_Kernel.Initialize(device_,
					fitToValidConstantBufferSize(
						sizeof(decltype(kernel)::value_type) * kernel.size()
					)
				);
			}

			GlobalTable_CBV_ = dxContext_.GlobalDescriptorHeap().Allocate(10U);
			DX12::CBV::Create(device_, GlobalTable_CBV_.CPUHandle(0U), UB_Constants_);
			DX12::CBV::Create(device_, GlobalTable_CBV_.CPUHandle(1U), UB_Offsets_);
			for (uint32_t idx{ 0U }; idx < 8U; ++idx) {
				DX12::CBV::Create(device_, GlobalTable_CBV_.CPUHandle(2U + idx), UBs_Kernel_[idx]);
			}

			uint32_t idx_KernelDist{ 0 };
			(
				[&] () {
					for (int32_t y{ 0 }; y < static_cast<int32_t>(kernelWidth_); ++y) {
						for (int32_t x{ 0 }; x < static_cast<int32_t>(kernelHeight_); ++x) {
							int32_t const idx{ y * static_cast<int32_t>(kernelWidth_) + x };
							kernel[idx] = kernelDists_(Vec2{ &Offsets_[idx].x });
							UBs_Kernel_[idx_KernelDist].Store(
								kernel.data(),
								sizeof(decltype(kernel)::value_type) * kernel.size(),
								0LLU
							);
						}
					}	
					++idx_KernelDist;
				} (),
				...
			);
		}

	private:
		DX12::RootSignature RS_;
		DX12::Shader PS_;
		DX12::GraphicsPSO PSO_;

		DX12::UploadBuffer UB_Constants_;
		DX12::UploadBuffer UB_Offsets_;
		DX12::UploadBuffer UBs_Kernel_[8];
		DX12::DescriptorTable GlobalTable_CBV_;

		std::array<Float2, 9 * 9> Offsets_;
	};
}