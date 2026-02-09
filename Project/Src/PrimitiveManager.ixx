export module Lumina.PrimitiveManager;

import <cstdint>;

import <d3d12.h>;

import nlohmann.json;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

import Lumina.Math.Numerics;
import Lumina.Math.Matrix;

import Lumina.Utils.Data;

namespace Lumina {
	export struct PrimitiveVertex {
		Float4 Position;
		Float4 Color;
		Float2 TexCoord;
		uint32_t TexID;
	};

	class LineManager {
	public:
		void Initialize(
			DX12::GraphicsDevice const& device_,
			NLohmannJSON const& config_,
			DX12::RootSignature const& rs_,
			DX12::Shader const& vs_,
			DX12::Shader const& ps_
		) {
			PSO_.Initialize(
				device_,
				rs_,
				vs_,
				ps_,
				DX12::LoadBlendState(config_.at("Line.PSO")),
				DX12::LoadRasterizerState(config_.at("Line.PSO")),
				DX12::LoadDepthStencilState(config_.at("Line.PSO")),
				DX12::LoadInputLayout(config_.at("Line.PSO")),
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM, },
				DX12::GraphicsPSO::DefaultDSVFormat
			);
			PSO_SRGB_.Initialize(
				device_,
				rs_,
				vs_,
				ps_,
				DX12::LoadBlendState(config_.at("Line.PSO")),
				DX12::LoadRasterizerState(config_.at("Line.PSO")),
				DX12::LoadDepthStencilState(config_.at("Line.PSO")),
				DX12::LoadInputLayout(config_.at("Line.PSO")),
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
				DX12::GraphicsPSO::DefaultDSVFormat
			);

			//DB_Vertices_.Initialize(device_, sizeof(PrimitiveVertex) * (MaxNum_ * 2U));
			UB_Vertices_.Initialize(device_, sizeof(PrimitiveVertex) * (MaxNum_ * 2U));
			VBV_ = DX12::VBV::Create<PrimitiveVertex>(UB_Vertices_);
		}

		void Batch(
			PrimitiveVertex const& vert0_,
			PrimitiveVertex const& vert1_
		) {
			UB_Vertices_.Store(&vert0_, sizeof(PrimitiveVertex), sizeof(PrimitiveVertex) * Count_ * 2U);
			UB_Vertices_.Store(&vert1_, sizeof(PrimitiveVertex), sizeof(PrimitiveVertex) * (Count_ * 2U + 1U));
			++Count_;
		}

		void End([[maybe_unused]] DX12::CommandList const& cmdList_) {
			/*D3D12_RESOURCE_BARRIER const barriers[]{
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
					},
				},
			};
			cmdList_->ResourceBarrier(1U, &barriers[0]);
			cmdList_->CopyBufferRegion(
				DB_Vertices_.Get(),
				0LLU,
				UB_Vertices_.Get(),
				0LLU,
				sizeof(PrimitiveVertex) * Count_ * 2U
			);
			cmdList_->ResourceBarrier(1U, &barriers[1]);*/
		}
		void Render(DX12::CommandList const& cmdList_, int32_t flag_SRGB_) {
			cmdList_->SetPipelineState((flag_SRGB_) ? (PSO_SRGB_.Get()) : (PSO_.Get()));
			cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			cmdList_->IASetVertexBuffers(0U, 1U, &VBV_);
			cmdList_->DrawInstanced(Count_ * 2U, 1U, 0U, 0U);

			Count_ = 0U;
		}

		void RenderBatched(DX12::CommandList const& cmdList_, int32_t flag_SRGB_) {
			/*D3D12_RESOURCE_BARRIER const barriers[]{
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
					},
				},
			};
			cmdList_->ResourceBarrier(1U, &barriers[0]);
			cmdList_->CopyBufferRegion(
				DB_Vertices_.Get(),
				0LLU,
				UB_Vertices_.Get(),
				0LLU,
				sizeof(PrimitiveVertex) * Count_ * 2U
			);
			cmdList_->ResourceBarrier(1U, &barriers[1]);*/
			cmdList_->SetPipelineState((flag_SRGB_) ? (PSO_SRGB_.Get()) : (PSO_.Get()));
			cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			cmdList_->IASetVertexBuffers(0U, 1U, &VBV_);
			cmdList_->DrawInstanced(Count_ * 2U, 1U, 0U, 0U);

			Count_ = 0U;
		}

	private:
		DX12::DefaultBuffer DB_Vertices_{};
		DX12::UploadBuffer UB_Vertices_{};
		DX12::VBV VBV_{};

		DX12::GraphicsPSO PSO_{};
		DX12::GraphicsPSO PSO_SRGB_{};

		uint32_t Count_{ 0U };

		static constinit inline uint32_t const MaxNum_{ 128U };
	};

	class TriangleManager {
	public:
		void Initialize(
			DX12::GraphicsDevice const& device_,
			NLohmannJSON const& config_,
			DX12::RootSignature const& rs_,
			DX12::Shader const& vs_,
			DX12::Shader const& ps_
		) {
			PSO_.Initialize(
				device_,
				rs_,
				vs_,
				ps_,
				DX12::LoadBlendState(config_.at("Triangle.PSO")),
				DX12::LoadRasterizerState(config_.at("Triangle.PSO")),
				DX12::LoadDepthStencilState(config_.at("Triangle.PSO")),
				DX12::LoadInputLayout(config_.at("Triangle.PSO")),
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM, },
				DX12::GraphicsPSO::DefaultDSVFormat
			);
			PSO_SRGB_.Initialize(
				device_,
				rs_,
				vs_,
				ps_,
				DX12::LoadBlendState(config_.at("Triangle.PSO")),
				DX12::LoadRasterizerState(config_.at("Triangle.PSO")),
				DX12::LoadDepthStencilState(config_.at("Triangle.PSO")),
				DX12::LoadInputLayout(config_.at("Triangle.PSO")),
				D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
				{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, },
				DX12::GraphicsPSO::DefaultDSVFormat
			);

			//DB_Vertices_.Initialize(device_, sizeof(PrimitiveVertex) * (MaxNum_ * 3U));
			UB_Vertices_.Initialize(device_, sizeof(PrimitiveVertex) * (MaxNum_ * 3U));
			VBV_ = DX12::VBV::Create<PrimitiveVertex>(UB_Vertices_);
		}

		void Batch(
			PrimitiveVertex const& vert0_,
			PrimitiveVertex const& vert1_,
			PrimitiveVertex const& vert2_
		) {
			UB_Vertices_.Store(&vert0_, sizeof(PrimitiveVertex), sizeof(PrimitiveVertex) * Count_ * 3U);
			UB_Vertices_.Store(&vert1_, sizeof(PrimitiveVertex), sizeof(PrimitiveVertex) * (Count_ * 3U + 1U));
			UB_Vertices_.Store(&vert2_, sizeof(PrimitiveVertex), sizeof(PrimitiveVertex) * (Count_ * 3U + 2U));
			++Count_;
		}
		void End([[maybe_unused]] DX12::CommandList const& cmdList_) {
			/*D3D12_RESOURCE_BARRIER const barriers[]{
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
					},
				},
			};
			cmdList_->ResourceBarrier(1U, &barriers[0]);
			cmdList_->CopyBufferRegion(
				DB_Vertices_.Get(),
				0LLU,
				UB_Vertices_.Get(),
				0LLU,
				sizeof(PrimitiveVertex) * Count_ * 3U
			);
			cmdList_->ResourceBarrier(1U, &barriers[1]);*/
		}
		void Render(DX12::CommandList const& cmdList_, int32_t flag_SRGB_) {
			cmdList_->SetPipelineState((flag_SRGB_) ? (PSO_SRGB_.Get()) : (PSO_.Get()));
			cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList_->IASetVertexBuffers(0U, 1U, &VBV_);
			cmdList_->DrawInstanced(Count_ * 3U, 1U, 0U, 0U);

			Count_ = 0U;
		}

		void RenderBatched(DX12::CommandList const& cmdList_, int32_t flag_SRGB_) {
			/*D3D12_RESOURCE_BARRIER const barriers[]{
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
				{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ DB_Vertices_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
					},
				},
			};
			cmdList_->ResourceBarrier(1U, &barriers[0]);
			cmdList_->CopyBufferRegion(
				DB_Vertices_.Get(),
				0LLU,
				UB_Vertices_.Get(),
				0LLU,
				sizeof(PrimitiveVertex) * Count_ * 3U
			);
			cmdList_->ResourceBarrier(1U, &barriers[1]);*/
			cmdList_->SetPipelineState((flag_SRGB_) ? (PSO_SRGB_.Get()) : (PSO_.Get()));
			cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList_->IASetVertexBuffers(0U, 1U, &VBV_);
			cmdList_->DrawInstanced(Count_ * 3U, 1U, 0U, 0U);

			Count_ = 0U;
		}

	private:
		DX12::DefaultBuffer DB_Vertices_{};
		DX12::UploadBuffer UB_Vertices_{};
		DX12::VBV VBV_{};

		DX12::GraphicsPSO PSO_{};
		DX12::GraphicsPSO PSO_SRGB_{};

		uint32_t Count_{ 0U };

		static constinit inline uint32_t const MaxNum_{ 128U };
	};

	export class PrimitiveManager {
	public:
		void Initialize(
			DX12::Context const& dx12Context_,
			std::wstring_view filePath_VS_ = L"Assets/Shaders/Basic.VS.hlsl",
			std::wstring_view filePath_PS_ = L"Assets/Shaders/Basic.PS.hlsl"
		) {
			auto config{ Utils::LoadFromFile<NLohmannJSON>("Assets/Configs/PrimitiveDrawer.json") };
			auto&& rsSetup{ DX12::LoadRootSignatureSetup(config.at("CommonRS")) };
			RS_.Initialize(dx12Context_.Device(), rsSetup);
			dx12Context_.Compile(
				VS_,
				filePath_VS_,
				L"vs_6_6",
				L"main",
				"Basic.VS"
			);
			dx12Context_.Compile(
				PS_,
				filePath_PS_,
				L"ps_6_6",
				L"main",
				"Basic.PS"
			);
			LineManager_.Initialize(dx12Context_.Device(), config, RS_, VS_, PS_);
			TriangleManager_.Initialize(dx12Context_.Device(), config, RS_, VS_, PS_);

			UB_VP_.Initialize(dx12Context_.Device(), (sizeof(Mat4) + 0xFFU) & ~0xFFU);
		}

		/// <summary>
		/// Begins batch.
		/// </summary>
		/// <param name="cmdList_">Command list</param>
		void Begin(
			DX12::CommandList const&
		) {
			//cmdList_->SetGraphicsRootSignature(RS_.Get());
		}

		void End(
			DX12::CommandList const& cmdList_,
			DX12::DescriptorTable const& texTable_,
			Mat4 const& vp_,
			int32_t flag_SRGB_ = 1
		) {
			UB_VP_.Store(&vp_, sizeof(Mat4), 0LLU);
			cmdList_->SetGraphicsRootConstantBufferView(0U, UB_VP_->GetGPUVirtualAddress());
			cmdList_->SetGraphicsRootDescriptorTable(1U, texTable_.GPUHandle(0U));

			LineManager_.RenderBatched(cmdList_, flag_SRGB_);
			TriangleManager_.RenderBatched(cmdList_, flag_SRGB_);
		}

		/// <summary>
		/// Ends batch.
		/// </summary>
		/// <param name="cmdList_">Command list</param>
		void End(
			DX12::CommandList const& cmdList_
		) {
			LineManager_.End(cmdList_);
			TriangleManager_.End(cmdList_);
		}

		/// <summary>
		/// Renders batched primitives.
		/// </summary>
		/// <param name="cmdList_">Command list</param>
		/// <param name="texTable_">Texture SRV table</param>
		/// <param name="vp_">World-to-NDC matrix</param>
		/// <param name="flag_SRGB_">SRGB format or not</param>
		/// <param name="cbv_">Scene constant CBV</param>
		void Render(
			DX12::CommandList const& cmdList_,
			DX12::DescriptorTable const& texTable_,
			Mat4 const& vp_,
			int32_t flag_SRGB_ = 1,
			D3D12_GPU_DESCRIPTOR_HANDLE cbv_ = { .ptr{ 0LLU } }
		) {
			UB_VP_.Store(&vp_, sizeof(Mat4), 0LLU);
			cmdList_->SetGraphicsRootSignature(RS_.Get());
			cmdList_->SetGraphicsRootConstantBufferView(0U, UB_VP_->GetGPUVirtualAddress());
			cmdList_->SetGraphicsRootDescriptorTable(1U, texTable_.GPUHandle(0U));
			if (cbv_.ptr != 0LLU) {
				cmdList_->SetGraphicsRootDescriptorTable(2U, cbv_);
			}

			LineManager_.Render(cmdList_, flag_SRGB_);
			TriangleManager_.Render(cmdList_, flag_SRGB_);
		}

		void BatchLine(
			PrimitiveVertex const& vert0_,
			PrimitiveVertex const& vert1_
		) {
			LineManager_.Batch(vert0_, vert1_);
		}

		void BatchTriangle(
			PrimitiveVertex const& vert0_,
			PrimitiveVertex const& vert1_,
			PrimitiveVertex const& vert2_
		) {
			TriangleManager_.Batch(vert0_, vert1_, vert2_);
		}

	private:
		LineManager LineManager_{};
		TriangleManager TriangleManager_{};

		DX12::RootSignature RS_{};
		DX12::Shader VS_{};
		DX12::Shader PS_{};

		DX12::UploadBuffer UB_VP_{};
	};
}