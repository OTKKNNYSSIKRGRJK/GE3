module Lumina.RadialRays;

import <algorithm>;
import <vector>;

import nlohmann.json;

import Lumina.DX12.Aux.View;
import Lumina.Utils.Data;

#undef max
#undef min

namespace Lumina::RadialRays {
	void Renderer::Initialize(
		DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_,
		float aspect_
	) {
		auto const& device{ dxContext_.Device() };
		Aspect_ = (aspect_ > 0.0f) ? aspect_ : (1280.0f / 720.0f);

		//----	------	------	------	------	----//
		// Root signature / Shaders
		//----	------	------	------	------	----//

		{
			auto settings{ Utils::LoadFromFile<nlohmann::json>("RadialRays.json", "Assets/Effects") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device, rsSetup, "RadialRays RS");
		}

		dxContext_.Compile(
			VS_,
			L"Assets/Effects/RadialRays.hlsl",
			L"vs_6_6",
			L"VSMain",
			"RadialRays.VS"
		);
		dxContext_.Compile(
			PS_,
			L"Assets/Effects/RadialRays.hlsl",
			L"ps_6_6",
			L"PSMain",
			"RadialRays.PS"
		);

		//----	------	------	------	------	----//
		// PSO（加算・深度なし・両面）
		//----	------	------	------	------	----//

		{
			DX12::BlendState blendState{};
			blendState.RenderTarget[0] = {
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_ONE },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_ZERO },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			// * 三角形は VS で SV_VertexID から生成
			DX12::GraphicsPipelineState::InputLayout inputLayout{};
			std::vector<DXGI_FORMAT> const rtvFormats{ rtvFormat_ };
			PSO_.Initialize(
				device,
				RS_,
				VS_,
				PS_,
				blendState,
				DX12::RasterizerState{
					.FillMode{ D3D12_FILL_MODE_SOLID },
					.CullMode{ D3D12_CULL_MODE_NONE },
				},
				DX12::DepthStencilState{
					.DepthEnable{ false },
					.StencilEnable{ false },
				},
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				rtvFormats,
				dsvFormat_,
				"RadialRays PSO"
			);
		}

		UB_Constants_.Initialize(device, ConstantsStride * MaxInstances, "RadialRays Constants");
	}

	void Renderer::Render(
		DX12::CommandList const& cmdList_,
		std::span<Instance const> instances_
	) {
		uint32_t num_Instances{ 0U };
		for (Instance const& inst : instances_) {
			if (num_Instances >= MaxInstances) { break; }
			if (!(inst.Intensity > 0.0f)) { continue; }		// NaN も弾く

			Look const& look{ inst.Appearance };
			ShaderConstants const constants{
				.Center{ inst.Center },
				.Intensity{ inst.Intensity },
				.Length{ std::max(inst.Length, 1e-4f) },

				.Color{ look.Color },
				.Time{ inst.Time },

				.RayCount{ static_cast<float>(std::max(look.RayCount, 1)) },
				.Sharpness{ std::max(look.Sharpness, 0.01f) },
				.RotateSpeed{ look.RotateSpeed },
				.FlickerSpeed{ std::max(look.FlickerSpeed, 0.0f) },

				.CoreColor{ look.CoreColor },
				.CoreRadius{ std::max(look.CoreRadius, 0.0001f) },

				.CoreIntensity{ std::max(look.CoreIntensity, 0.0f) },
				.Aspect{ Aspect_ },
				.Padding0{ 0.0f },
				.Padding1{ 0.0f },

				.Padding2{},
			};
			UB_Constants_.Store(&constants, sizeof(ShaderConstants), ConstantsStride * num_Instances);
			++num_Instances;
		}
		if (num_Instances == 0U) { return; }

		D3D12_GPU_VIRTUAL_ADDRESS const cbBase{ UB_Constants_->GetGPUVirtualAddress() };

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// * 加算なので順序は問わない
		for (uint32_t idx{ 0U }; idx < num_Instances; ++idx) {
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbBase + ConstantsStride * idx);
			cmdList_->DrawInstanced(3U, 1U, 0U, 0U);
		}
	}
}
