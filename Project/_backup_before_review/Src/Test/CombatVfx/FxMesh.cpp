module Lumina.FxMesh;

import <algorithm>;
import <vector>;

import nlohmann.json;

import Lumina.DX12.Aux.View;
import Lumina.Utils.Data;

#undef max
#undef min

namespace Lumina::FxMesh {
	void Renderer::Initialize(
		DX12::Context const& dxContext_,
		DXGI_FORMAT rtvFormat_,
		DXGI_FORMAT dsvFormat_,
		std::uint32_t maxVertices_
	) {
		auto const& device{ dxContext_.Device() };
		MaxVertices_ = std::max(maxVertices_ - maxVertices_ % 3U, 3U);
		Vertices_.reserve(MaxVertices_);

		{
			auto const settings = Utils::LoadFromFile<nlohmann::json>("FxMesh.json", "Assets/Effects");	// = で受ける（波かっこだと nlohmann::json は配列になるコンパイラがある）
			auto const rsSetup = DX12::LoadRootSignatureSetup(settings.at("RS"));
			RS_.Initialize(device, rsSetup, "FxMesh RS");
		}

		dxContext_.Compile(VS_, L"Assets/Effects/FxMesh.hlsl", L"vs_6_6", L"VSMain", "FxMesh.VS");
		dxContext_.Compile(PS_, L"Assets/Effects/FxMesh.hlsl", L"ps_6_6", L"PSMain", "FxMesh.PS");

		// 加算（ONE / ONE）。α は書かない
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

		DX12::GraphicsPipelineState::InputLayout inputLayout{};		// 空（VS が SV_VertexID で StructuredBuffer を読む）
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
			"FxMesh PSO"
		);

		UB_Vertices_.Initialize(device, sizeof(Vertex) * static_cast<std::uint64_t>(MaxVertices_), "FxMesh Vertices");
		UB_Constants_.Initialize(device, 256LLU, "FxMesh Constants");
	}

	void Renderer::Clear() noexcept {
		Vertices_.clear();
		Dropped_ = 0U;
	}

	void Renderer::Triangle(Vertex const& v0_, Vertex const& v1_, Vertex const& v2_) {
		if (Vertices_.size() + 3U > MaxVertices_) {
			++Dropped_;
			return;
		}
		Vertices_.push_back(v0_);
		Vertices_.push_back(v1_);
		Vertices_.push_back(v2_);
	}

	void Renderer::Quad(Vertex const& v00_, Vertex const& v10_, Vertex const& v01_, Vertex const& v11_) {
		Triangle(v00_, v10_, v01_);
		Triangle(v10_, v11_, v01_);
	}

	void Renderer::Render(
		DX12::CommandList const& cmdList_,
		Mat4 const& viewProjection_,
		float nearZ_,
		float farZ_,
		float time_,
		D3D12_GPU_DESCRIPTOR_HANDLE depthSRV_
	) {
		if (Vertices_.empty()) { return; }

		UB_Vertices_.Store(Vertices_.data(), sizeof(Vertex) * Vertices_.size(), 0LLU);

		// FxMesh.hlsl の SCENE と同じ並び（行列 64 バイト＋ float 4 つ）。Mat4 は align(32) なので構造体にせず、分けて写す
		float const params[4]{ nearZ_, farZ_, time_, 0.0f };
		UB_Constants_.Store(&viewProjection_, sizeof(Mat4), 0LLU);
		UB_Constants_.Store(params, sizeof(params), sizeof(Mat4));

		cmdList_->SetGraphicsRootSignature(RS_.Get());
		cmdList_->SetPipelineState(PSO_.Get());
		cmdList_->SetGraphicsRootConstantBufferView(0U, UB_Constants_->GetGPUVirtualAddress());
		cmdList_->SetGraphicsRootShaderResourceView(1U, UB_Vertices_->GetGPUVirtualAddress());
		cmdList_->SetGraphicsRootDescriptorTable(2U, depthSRV_);
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList_->DrawInstanced(static_cast<std::uint32_t>(Vertices_.size()), 1U, 0U, 0U);
	}
}
