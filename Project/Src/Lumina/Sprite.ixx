export module Lumina.Sprite;

import <cstdint>;
import <type_traits>;
import <algorithm>;

import Lumina.Container.List;

import Lumina.Math;

import Lumina.Utils.Data;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import nlohmann.json;

namespace Lumina {
	export class Sprite {
	public:
		auto Scale() const noexcept -> Vec2 { return Scale_; }
		auto Scale(Vec2&& scale_) noexcept -> void { Scale_ = scale_; }
		auto Rotate() const noexcept -> float { return Rotate_; }
		auto Rotate(float rotate_) noexcept -> void { Rotate_ = rotate_; }
		auto Translate() const noexcept -> Vec2 { return Translate_; }
		auto Translate(Vec2&& translate_) noexcept -> void { Translate_ = translate_; }

		auto AnchorPoint() const noexcept -> Vec2 { return AnchorPoint_; }
		auto AnchorPoint(Vec2&& anchorPoint_) noexcept -> void { AnchorPoint_ = anchorPoint_; }

		auto TextureID() const noexcept -> uint32_t { return TextureID_; }
		auto TextureID(uint32_t texID_) noexcept -> void { TextureID_ = texID_; }
		auto UV(uint32_t idx_) const noexcept -> Vec2 { return UVs_[idx_]; }
		auto UV(uint32_t idx_, Vec2&& uv_) noexcept -> void { UVs_[idx_] = uv_; }
		auto RGBA() const noexcept -> Float4 { return RGBA_; }
		auto RGBA(Float4&& rgba_) noexcept -> void { RGBA_ = rgba_; }

	protected:
		Vec2 Scale_{ 1.0f, 1.0f };
		float Rotate_{ 0.0f };
		Vec2 Translate_{ 0.0f, 0.0f };

		Vec2 AnchorPoint_{ 0.0f, 0.0f };

		Vec2 UVs_[4]{ { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };
		Float4 RGBA_{ 1.0f, 1.0f, 1.0f, 1.0f };
		uint32_t TextureID_;
	};

	export class SpriteRenderer {
		struct Material {
			Float4 RGBA;
			uint32_t TextureID;
		};

		struct QuadUVs {
			Vec2 UVs[4];
		};

	public:
		DX12::RootSignature const& RootSignature() const noexcept { return RS_; }

	public:
		void Initialize(
			DX12::Context const& dx12Context_,
			uint32_t maxNum_Batches_
		) {
			MaxNum_Batches_ = std::clamp(maxNum_Batches_, 256U, 4096U);

			DX12Context_ = &dx12Context_;

			auto const& device{ DX12Context_->Device() };
			auto const& globalHeap{ DX12Context_->GlobalDescriptorHeap() };

			globalHeap.Allocate(Table_Materials_, 1U);
			globalHeap.Allocate(Table_UVs_, 1U);
			globalHeap.Allocate(Table_VP_And_Worlds_, 2U);

			DB_VP_.Initialize(device, 256LLU);
			UB_VP_.Initialize(device, DB_VP_.SizeInBytes());
			DX12::CBV::Create(device, Table_VP_And_Worlds_.CPUHandle(0U), DB_VP_);
			DB_Worlds_.Initialize(device, sizeof(Mat4) * MaxNum_Batches_);
			UB_Worlds_.Initialize(device, DB_Worlds_.SizeInBytes());
			DX12::SRV<Mat4>::Create(device, Table_VP_And_Worlds_.CPUHandle(1U), DB_Worlds_);

			DB_UVs_.Initialize(device, sizeof(QuadUVs) * MaxNum_Batches_);
			UB_UVs_.Initialize(device, DB_UVs_.SizeInBytes());
			DX12::SRV<QuadUVs>::Create(device, Table_UVs_.CPUHandle(0U), DB_UVs_);
			DB_Materials_.Initialize(device, (sizeof(uint32_t) + sizeof(Float4)) * MaxNum_Batches_);
			UB_Materials_.Initialize(device, DB_Materials_.SizeInBytes());
			DX12::SRV<Material>::Create(device, Table_Materials_.CPUHandle(0U), DB_Materials_);

			auto config{ Utils::LoadFromFile<NLohmannJSON>("Assets/Configs/Sprite.json") };
			auto&& rsSetup{ DX12::LoadRootSignatureSetup(config.at("RS")) };
			RS_.Initialize(device, rsSetup);

			struct ParticleSpriteVertex {
				Float4 Position;
			};

			QuadVertexBuffer_.Initialize(device, sizeof(ParticleSpriteVertex) * 4U);
			QuadVBV_ = DX12::VBV::Create<ParticleSpriteVertex>(QuadVertexBuffer_);
			ParticleSpriteVertex quadVerts[4]{
				{ .Position{ 0.0f, 0.0f, 0.0f, 1.0f }, },
				{ .Position{ 1.0f, 0.0f, 0.0f, 1.0f }, },
				{ .Position{ 0.0f, 1.0f, 0.0f, 1.0f }, },
				{ .Position{ 1.0f, 1.0f, 0.0f, 1.0f }, },
			};
			DX12::UploadBuffer vbTmp{};
			vbTmp.Initialize(device, sizeof(ParticleSpriteVertex) * 4U);
			vbTmp.Store(quadVerts, QuadVertexBuffer_.SizeInBytes(), 0LLU);

			QuadIndexBuffer_.Initialize(device, sizeof(uint32_t) * 6U);
			QuadIBV_ = Lumina::DX12::IBV::Create(QuadIndexBuffer_);
			uint32_t quadIndices[6]{ 0U, 1U, 2U, 1U, 3U, 2U, };
			DX12::UploadBuffer ibTmp{};
			ibTmp.Initialize(device, sizeof(uint32_t) * 6U);
			ibTmp.Store(quadIndices, QuadIndexBuffer_.SizeInBytes(), 0LLU);

			DX12::CommandAllocator cmdAllocator{};
			cmdAllocator.Initialize(device);
			DX12::CommandList cmdList{};
			cmdList.Initialize(device, cmdAllocator);
			cmdList->CopyResource(QuadVertexBuffer_.Get(), vbTmp.Get());
			cmdList->CopyResource(QuadIndexBuffer_.Get(), ibTmp.Get());
			
			DX12Context_->DirectQueue() << cmdList;
			DX12Context_->DirectQueue().CPUWait(
				DX12Context_->DirectQueue().ExecuteBatchedCommandLists()
			);
		}

		void Begin(DX12::CommandList const& cmdList_) {
			CommandList_ = cmdList_.Get();
			Count_UnuploadedBatches_ = 0U;
			Count_UploadedBatches_ = 0U;
		}

		void End() {
			Count_UnuploadedBatches_ = 0U;
			Count_UploadedBatches_ = 0U;
		}

		void BatchBegin() {
			D3D12_RESOURCE_BARRIER const barriers[]{
				D3D12_RESOURCE_BARRIER{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Transition{
						.pResource{ DB_Worlds_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
				D3D12_RESOURCE_BARRIER{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Transition{
						.pResource{ DB_UVs_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
				D3D12_RESOURCE_BARRIER{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Transition{
						.pResource{ DB_Materials_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
						.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
					},
				},
			};
			CommandList_->ResourceBarrier(3U, barriers);
		}

		void Batch(
			Sprite const& sprite_
		) {
			uint32_t const idx{ Count_UnuploadedBatches_ };

			Mat4 world{};
			world[3][0] = -sprite_.AnchorPoint().x;
			world[3][1] = -sprite_.AnchorPoint().y;
			Mat4::Multiply(
				world,
				world,
				Mat4::SRT(
					{ sprite_.Scale() },
					{ 0.0f, 0.0f, sprite_.Rotate() },
					{ sprite_.Translate() }
				)
			);
			UB_Worlds_.Store(
				&world,
				sizeof(Mat4),
				sizeof(Mat4) * (idx + Count_UploadedBatches_)
			);

			QuadUVs&& uvs{
				sprite_.UV(0),
				sprite_.UV(1),
				sprite_.UV(2),
				sprite_.UV(3),
			};
			UB_UVs_.Store(
				&uvs,
				sizeof(QuadUVs),
				sizeof(QuadUVs) * (idx + Count_UploadedBatches_)
			);

			Material material{
				.RGBA{ sprite_.RGBA() },
				.TextureID{ sprite_.TextureID() },
			};
			UB_Materials_.Store(
				&material,
				sizeof(Material),
				sizeof(Material) * (idx + Count_UploadedBatches_)
			);

			++Count_UnuploadedBatches_;
		}

		void BatchEnd() {
			if (Count_UnuploadedBatches_ == 0U) {
				return;
			}

			CommandList_->CopyBufferRegion(
				DB_Worlds_.Get(),
				0LLU,
				UB_Worlds_.Get(),
				sizeof(Mat4) * Count_UploadedBatches_,
				sizeof(Mat4) * Count_UnuploadedBatches_
			);
			CommandList_->CopyBufferRegion(
				DB_UVs_.Get(),
				0LLU,
				UB_UVs_.Get(),
				sizeof(QuadUVs) * Count_UploadedBatches_,
				sizeof(QuadUVs) * Count_UnuploadedBatches_
			);
			CommandList_->CopyBufferRegion(
				DB_Materials_.Get(),
				0LLU,
				UB_Materials_.Get(),
				sizeof(Material) * Count_UploadedBatches_,
				sizeof(Material) * Count_UnuploadedBatches_
			);

			D3D12_RESOURCE_BARRIER const barriers[]{
				D3D12_RESOURCE_BARRIER{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Transition{
						.pResource{ DB_Worlds_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
					},
				},
				D3D12_RESOURCE_BARRIER{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Transition{
						.pResource{ DB_UVs_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
					},
				},
				D3D12_RESOURCE_BARRIER{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Transition{
						.pResource{ DB_Materials_.Get() },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
					},
				},
			};
			CommandList_->ResourceBarrier(3U, barriers);

			Count_UploadedBatches_ += Count_UnuploadedBatches_;
		}

		void Render(
			DX12::GraphicsPipelineState const& pso_,
			D3D12_GPU_DESCRIPTOR_HANDLE globalSRV_TextureStart_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_
		) {
			DX12Context_->Device()->CopyDescriptorsSimple(
				1U,
				Table_VP_And_Worlds_.CPUHandle(0U),
				localCBV_VP_,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);

			if (Count_UnuploadedBatches_ == 0U) {
				return;
			}

			CommandList_->SetGraphicsRootSignature(RS_.Get());
			CommandList_->SetPipelineState(pso_.Get());
			CommandList_->SetGraphicsRootDescriptorTable(
				0U,
				Table_UVs_.GPUHandle(0U)
			);
			CommandList_->SetGraphicsRootDescriptorTable(
				1U,
				Table_VP_And_Worlds_.GPUHandle(0U)
			);
			CommandList_->SetGraphicsRootDescriptorTable(
				2U,
				Table_Materials_.GPUHandle(0U)
			);
			CommandList_->SetGraphicsRootDescriptorTable(
				3U,
				globalSRV_TextureStart_
			);

			CommandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			CommandList_->IASetVertexBuffers(0U, 1U, &QuadVBV_);
			CommandList_->IASetIndexBuffer(&QuadIBV_);
			CommandList_->DrawIndexedInstanced(
				6U,
				Count_UnuploadedBatches_,
				0U,
				0U,
				0U
			);
		}

	public:
		SpriteRenderer() {}

		~SpriteRenderer() {}

	private:
		DX12::RootSignature RS_{};

		DX12::DescriptorTable Table_Materials_{};
		DX12::DescriptorTable Table_UVs_{};
		DX12::DescriptorTable Table_VP_And_Worlds_{};

		DX12::DefaultBuffer DB_Materials_{};
		DX12::UploadBuffer UB_Materials_{};
		DX12::DefaultBuffer DB_UVs_{};
		DX12::UploadBuffer UB_UVs_{};
		DX12::DefaultBuffer DB_Worlds_{};
		DX12::UploadBuffer UB_Worlds_{};
		DX12::DefaultBuffer DB_VP_{};
		DX12::UploadBuffer UB_VP_{};

		uint32_t Count_UnuploadedBatches_{ 0U };
		uint32_t Count_UploadedBatches_{ 0U };
		uint32_t Count_BatchedVertices_{ 0U };

		DX12::Context const* DX12Context_{ nullptr };
		ID3D12GraphicsCommandList* CommandList_{ nullptr };

		uint32_t MaxNum_Batches_{ 2048U };

		Lumina::DX12::DefaultBuffer QuadVertexBuffer_{};
		Lumina::DX12::DefaultBuffer QuadIndexBuffer_{};
		D3D12_VERTEX_BUFFER_VIEW QuadVBV_{};
		D3D12_INDEX_BUFFER_VIEW QuadIBV_{};
	};
}