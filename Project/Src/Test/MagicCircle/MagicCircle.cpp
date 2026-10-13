module Lumina.MagicCircle;

import <cmath>;
import <algorithm>;
import <string_view>;

import nlohmann.json;

import Lumina.DX12.Aux.View;
import Lumina.Utils.Data;

#undef max
#undef min

namespace Lumina {
	namespace {
		auto SRGBToLinear(float c_) -> float {
			return (c_ <= 0.04045f) ? (c_ / 12.92f) : std::pow((c_ + 0.055f) / 1.055f, 2.4f);
		}
		auto SRGBToLinear(Float3 const& c_) -> Float3 {
			return Float3{ SRGBToLinear(c_.x), SRGBToLinear(c_.y), SRGBToLinear(c_.z) };
		}
		auto ClampCount(int value_, int min_, int max_) -> uint32_t {
			return static_cast<uint32_t>(std::clamp(value_, min_, max_));
		}
	}

	void MagicCircle::Renderer::Initialize(
		DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_
	) {
		auto const& device{ dxContext_.Device() };

		//----	------	------	------	------	----//
		// Root signature / Shaders
		//----	------	------	------	------	----//

		{
			auto settings{ Utils::LoadFromFile<nlohmann::json>("MagicCircle.json", "Assets/Title") };
			auto rsSetup{ DX12::LoadRootSignatureSetup(settings.at("RS")) };
			RS_.Initialize(device, rsSetup, "MagicCircle RS");
		}

		dxContext_.Compile(
			VS_,
			L"Assets/Title/MagicCircle.hlsl",
			L"vs_6_6",
			L"VSMain",
			"MagicCircle.VS"
		);
		dxContext_.Compile(
			PS_,
			L"Assets/Title/MagicCircle.hlsl",
			L"ps_6_6",
			L"PSMain",
			"MagicCircle.PS"
		);

		//----	------	------	------	------	----//
		// PSO（加算・深度書き込みなし・両面）
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
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};

			DX12::RasterizerState rasterizerState{
				.FillMode{ D3D12_FILL_MODE_SOLID },
				.CullMode{ D3D12_CULL_MODE_NONE },
				.FrontCounterClockwise{ false },
				.DepthClipEnable{ true },
			};

			DX12::DepthStencilState depthStencilState{
				.DepthEnable{ dsvFormat_ != DXGI_FORMAT_UNKNOWN },
				.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ZERO },
				.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
			};

			// * クアッドは VS で SV_VertexID から生成
			DX12::GraphicsPipelineState::InputLayout inputLayout{};

			std::vector<DXGI_FORMAT> const rtvFormats{ rtvFormat_ };

			PSO_.Initialize(
				device,
				RS_,
				VS_,
				PS_,
				blendState,
				rasterizerState,
				depthStencilState,
				inputLayout,
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				rtvFormats,
				dsvFormat_,
				"MagicCircle PSO"
			);
		}

		//----	------	------	------	------	----//
		// Constant buffer（レイヤーごとに 256 バイト境界のスロット、Root CBV で指す）
		//----	------	------	------	------	----//

		UB_Constants_.Initialize(device, ConstantsStride * MaxLayers, "MagicCircle Constants");
	}

	void MagicCircle::Renderer::StoreConstants(
		uint32_t slot_,
		Layer const& layer_,
		Float3 const& center_,
		float time_,
		float intensityScale_,
		Mat4 const& viewProj_
	) {
		uint32_t const polygonVertices{ ClampCount(layer_.PolygonVertices, 3, 16) };
		// K は 1..N-1。範囲外なら普通の多角形にする
		uint32_t const polygonStep{
			(layer_.PolygonStep >= 1 && layer_.PolygonStep < static_cast<int>(polygonVertices))
				? static_cast<uint32_t>(layer_.PolygonStep)
				: 1U
		};

		ShaderConstants const shaderConstants{
			.ViewProj{ viewProj_ },

			.Center{ center_ },
			.Radius{ std::max(layer_.Radius, 0.01f) },

			.Height{ layer_.Height },
			.Angle{ layer_.Phase + layer_.RotateSpeed * time_ },
			.Reveal{ std::clamp(layer_.Reveal, 0.0f, 1.0f) },
			.Intensity{ layer_.Intensity * intensityScale_ },

			.Color{ SRGBToLinear(layer_.Color) },
			.LineWidth{ std::max(layer_.LineWidth, 0.0f) },

			.GlowAmount{ std::max(layer_.GlowAmount, 0.0f) },
			.GlowWidth{ std::max(layer_.GlowWidth, 0.0005f) },
			.FillAmount{ std::max(layer_.FillAmount, 0.0f) },
			.Extent{ QuadExtent },

			.Ring0{ layer_.Ring0 },
			.Ring1{ layer_.Ring1 },
			.PolygonRadius{ layer_.PolygonRadius },
			.NodeRadius{ layer_.NodeRadius },

			.TickInner{ layer_.TickInner },
			.TickOuter{ layer_.TickOuter },
			.SpokeInner{ layer_.SpokeInner },
			.SpokeOuter{ layer_.SpokeOuter },

			.RuneInner{ std::min(layer_.RuneInner, layer_.RuneOuter) },
			.RuneOuter{ std::max(layer_.RuneInner, layer_.RuneOuter) },
			.RuneDensity{ std::clamp(layer_.RuneDensity, 0.0f, 1.0f) },
			.RuneShimmer{ std::clamp(layer_.RuneShimmer, 0.0f, 1.0f) },

			.Elements{ layer_.Elements },
			.PolygonVertices{ polygonVertices },
			.PolygonStep{ polygonStep },
			.TickCount{ ClampCount(layer_.TickCount, 1, 360) },

			.SpokeCount{ ClampCount(layer_.SpokeCount, 1, 128) },
			.RuneCount{ ClampCount(layer_.RuneCount, 1, 128) },
			.RuneSeed{ static_cast<uint32_t>(layer_.RuneSeed) },
			.Padding0{ 0U },

			.Time{ time_ },
			.Padding1{ 0.0f },
			.Padding2{ 0.0f },
			.Padding3{ 0.0f },
		};

		UB_Constants_.Store(
			&shaderConstants,
			sizeof(ShaderConstants),
			ConstantsStride * slot_
		);
	}

	void MagicCircle::Renderer::Render(
		DX12::CommandList const& cmdList_,
		std::span<Layer const> layers_,
		Float3 const& center_,
		float time_,
		float intensityScale_,
		Mat4 const& viewProj_
	) {
		uint32_t num_Layers{ 0U };
		for (auto const& layer : layers_) {
			if (num_Layers >= MaxLayers) { break; }
			if (!layer.Enabled) { continue; }
			StoreConstants(num_Layers, layer, center_, time_, intensityScale_, viewProj_);
			++num_Layers;
		}
		if (num_Layers == 0U) { return; }

		D3D12_GPU_VIRTUAL_ADDRESS const cbBase{ UB_Constants_->GetGPUVirtualAddress() };

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// * 加算なので順序は問わない
		for (uint32_t idx{ 0U }; idx < num_Layers; ++idx) {
			cmdList_->SetGraphicsRootConstantBufferView(0U, cbBase + ConstantsStride * idx);
			cmdList_->DrawInstanced(6U, 1U, 0U, 0U);
		}
	}
}
