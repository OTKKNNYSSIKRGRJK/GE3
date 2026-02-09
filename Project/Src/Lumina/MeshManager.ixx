export module Lumina.MeshManager;

import <cstdint>;

import <memory>;

import <vector>;

import <string>;

import <d3d12.h>;

import nlohmann.json;

import Lumina.Math;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;

import Lumina.Utils.Data;
import Lumina.Utils.Data.Mesh;

import Lumina.Utils.ImGui;

namespace Lumina {
	export class MeshShaderAsset {
		friend class MeshUploader;

	public:
		inline DX12::DescriptorHeap const& LocalDescriptors() const noexcept { return *LocalHeap_; }
		inline DX12::DefaultBuffer const& VertexBuffer() const noexcept { return *VertexBuffer_; }
		constexpr uint32_t Num_Vertices() const noexcept { return Num_Vertices_; }

	private:
		std::unique_ptr<DX12::DefaultBuffer> Positions_{};
		std::unique_ptr<DX12::DefaultBuffer> TexCoords_{};
		std::unique_ptr<DX12::DefaultBuffer> Normals_{};
		std::unique_ptr<DX12::DefaultBuffer> Tangents_{};
		std::unique_ptr<DX12::DescriptorHeap> LocalHeap_{};

		std::unique_ptr<DX12::DefaultBuffer> VertexBuffer_{};
		DX12::VBV VBV_{};
		
		uint32_t Num_Vertices_{};
	};

	export class MeshUploader {
	public:
		void Begin() {
			MeshShaderAssets_.clear();
			UploadBuffers_.clear();
		}

		void Batch(Utils::Mesh const& mesh_) {
			/*auto const& device{ dx12Context_.Device() };
			auto& cmdQueue{ dx12Context_.DirectQueue() };

			Num_Vertices_ = static_cast<uint32_t>(modelData.Vertices.size());*/

			auto const& device{ DX12Context_->Device() };

			auto& meshShaderAsset{ MeshShaderAssets_.emplace_back() };
			
			meshShaderAsset.LocalHeap_.reset(new DX12::DescriptorHeap{});
			meshShaderAsset.LocalHeap_->Initialize(
				device,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				4U,
				false
			);

			meshShaderAsset.Positions_.reset(new DX12::DefaultBuffer{});
			meshShaderAsset.Positions_->Initialize(
				device,
				sizeof(Float3) * mesh_.Positions.size()
			);
			DX12::SRV<Float3>::Create(
				device,
				meshShaderAsset.LocalHeap_->CPUHandle(0U),
				*meshShaderAsset.Positions_
			);
			auto& uploadBuf_Positions{ UploadBuffers_.emplace_back() };
			uploadBuf_Positions.reset(new DX12::UploadBuffer{});
			uploadBuf_Positions->Initialize(
				device,
				meshShaderAsset.Positions_->SizeInBytes()
			);
			uploadBuf_Positions->Store(
				mesh_.Positions.data(),
				meshShaderAsset.Positions_->SizeInBytes(),
				0LLU
			);
			CommandList_Upload_->CopyResource(
				meshShaderAsset.Positions_->Get(),
				uploadBuf_Positions->Get()
			);

			meshShaderAsset.TexCoords_.reset(new DX12::DefaultBuffer{});
			meshShaderAsset.TexCoords_->Initialize(
				device,
				sizeof(Float2) * mesh_.TexCoords.size()
			);
			DX12::SRV<Float2>::Create(
				device,
				meshShaderAsset.LocalHeap_->CPUHandle(1U),
				*meshShaderAsset.TexCoords_
			);
			auto& uploadBuf_TexCoords{ UploadBuffers_.emplace_back() };
			uploadBuf_TexCoords.reset(new DX12::UploadBuffer{});
			uploadBuf_TexCoords->Initialize(
				device,
				meshShaderAsset.TexCoords_->SizeInBytes()
			);
			uploadBuf_TexCoords->Store(
				mesh_.TexCoords.data(),
				meshShaderAsset.TexCoords_->SizeInBytes(),
				0LLU
			);
			CommandList_Upload_->CopyResource(
				meshShaderAsset.TexCoords_->Get(),
				uploadBuf_TexCoords->Get()
			);

			meshShaderAsset.Normals_.reset(new DX12::DefaultBuffer{});
			meshShaderAsset.Normals_->Initialize(
				device,
				sizeof(Float3) * mesh_.Normals.size()
			);
			DX12::SRV<Float3>::Create(
				device,
				meshShaderAsset.LocalHeap_->CPUHandle(2U),
				*meshShaderAsset.Normals_
			);
			auto& uploadBuf_Normals{ UploadBuffers_.emplace_back() };
			uploadBuf_Normals.reset(new DX12::UploadBuffer{});
			uploadBuf_Normals->Initialize(
				device,
				meshShaderAsset.Normals_->SizeInBytes()
			);
			uploadBuf_Normals->Store(
				mesh_.Normals.data(),
				meshShaderAsset.Normals_->SizeInBytes(),
				0LLU
			);
			CommandList_Upload_->CopyResource(
				meshShaderAsset.Normals_->Get(),
				uploadBuf_Normals->Get()
			);

			meshShaderAsset.Tangents_.reset(new DX12::DefaultBuffer{});
			meshShaderAsset.Tangents_->Initialize(
				device,
				sizeof(Float3) * mesh_.Tangents.size()
			);
			DX12::SRV<Float3>::Create(
				device,
				meshShaderAsset.LocalHeap_->CPUHandle(3U),
				*meshShaderAsset.Tangents_
			);
			auto& uploadBuf_Tangents{ UploadBuffers_.emplace_back() };
			uploadBuf_Tangents.reset(new DX12::UploadBuffer{});
			uploadBuf_Tangents->Initialize(
				device,
				meshShaderAsset.Tangents_->SizeInBytes()
			);
			uploadBuf_Tangents->Store(
				mesh_.Tangents.data(),
				meshShaderAsset.Tangents_->SizeInBytes(),
				0LLU
			);
			CommandList_Upload_->CopyResource(
				meshShaderAsset.Tangents_->Get(),
				uploadBuf_Tangents->Get()
			);

			meshShaderAsset.Num_Vertices_ = static_cast<uint32_t>(mesh_.Vertices.size());
			meshShaderAsset.VertexBuffer_.reset(new DX12::DefaultBuffer{});
			meshShaderAsset.VertexBuffer_->Initialize(
				DX12Context_->Device(),
				sizeof(Utils::Mesh::Vertex) * meshShaderAsset.Num_Vertices_,
				std::format("{}.VB", mesh_.Name)
			);
			meshShaderAsset.VBV_ = DX12::VBV::Create<Utils::Mesh::Vertex>(*meshShaderAsset.VertexBuffer_);
			auto& uploadBuf_Vertices{ UploadBuffers_.emplace_back() };
			uploadBuf_Vertices.reset(new DX12::UploadBuffer{});
			uploadBuf_Vertices->Initialize(
				DX12Context_->Device(),
				meshShaderAsset.VertexBuffer_->SizeInBytes()
			);
			uploadBuf_Vertices->Store(
				mesh_.Vertices.data(),
				meshShaderAsset.VertexBuffer_->SizeInBytes(),
				0LLU
			);
			CommandList_Upload_->CopyResource(
				meshShaderAsset.VertexBuffer_->Get(),
				uploadBuf_Vertices->Get()
			);
		}

		void End(std::vector<MeshShaderAsset>& assets_) {
			if (!assets_.empty()) {
				std::vector<D3D12_RESOURCE_BARRIER> barriers_AfterCopy{};
				for (auto const& meshShaderAsset : MeshShaderAssets_) {
					barriers_AfterCopy.emplace_back(
						DX12::Barrier::Transition(
							*meshShaderAsset.VertexBuffer_,
							D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_COPY_SOURCE
						)
					);
				};
				CommandList_Upload_->ResourceBarrier(
					static_cast<uint32_t>(barriers_AfterCopy.size()),
					barriers_AfterCopy.data()
				);
			}

			auto& cmdQueue{ DX12Context_->DirectQueue() };
			cmdQueue << CommandList_Upload_;
			cmdQueue.CPUWait(cmdQueue.ExecuteBatchedCommandLists());
			CommandList_Upload_.Reset(CommandAllocator_Upload_);

			assets_.swap(MeshShaderAssets_);
		}

	public:
		void Initialize(DX12::Context const& dx12Context_) {
			DX12Context_ = &dx12Context_;
			CommandAllocator_Upload_.Initialize(DX12Context_->Device());
			CommandList_Upload_.Initialize(DX12Context_->Device(), CommandAllocator_Upload_);
		}

	private:
		std::vector<MeshShaderAsset> MeshShaderAssets_{};

		DX12::Context const* DX12Context_{ nullptr };
		DX12::CommandAllocator CommandAllocator_Upload_{};
		DX12::CommandList CommandList_Upload_{};
		std::vector<std::unique_ptr<DX12::UploadBuffer>> UploadBuffers_{};
	};
}

namespace Lumina {
	export class MeshManager {
	public:
		enum class BlendMode : uint32_t {
			PremultipliedAlphaBlend,
			StraightAlphaBlend,
			Additive,
			Subtract,
			Multiply,
			Screen,

			Count,
		};

		std::vector<std::string> BlendModeNames_{};

	public:
		DX12::RootSignature const& RootSignature() const noexcept { return RS_; }

	public:
		void Initialize(
			DX12::Context const& dx12Context_,
			uint32_t maxNum_Batches_,
			uint32_t maxNum_BatchedVertices_
		) {
			MaxNum_Batches_ = std::clamp(maxNum_Batches_, 256U, 4096U);
			MaxNum_BatchedVertices_ = std::clamp(maxNum_BatchedVertices_, 1024U, 4194304U);

			DX12Context_ = &dx12Context_;

			auto const& device{ DX12Context_->Device() };
			auto const& globalHeap{ DX12Context_->GlobalDescriptorHeap() };

			globalHeap.Allocate(Table_Materials_, MaxNum_Batches_);
			globalHeap.Allocate(Table_PositionArrays_, MaxNum_Batches_);
			globalHeap.Allocate(Table_TexCoordArrays_, MaxNum_Batches_);
			globalHeap.Allocate(Table_NormalArrays_, MaxNum_Batches_);
			globalHeap.Allocate(Table_TangentArrays_, MaxNum_Batches_);
			globalHeap.Allocate(Table_VP_And_Worlds_, 2U);

			DB_VP_.Initialize(device, 256LLU);
			UB_VP_.Initialize(device, DB_VP_.SizeInBytes());
			DX12::CBV::Create(device, Table_VP_And_Worlds_.CPUHandle(0U), DB_VP_);
			DB_Worlds_.Initialize(device, sizeof(Mat4) * MaxNum_Batches_);
			UB_Worlds_.Initialize(device, DB_Worlds_.SizeInBytes());
			DX12::SRV<Mat4>::Create(device, Table_VP_And_Worlds_.CPUHandle(1U), DB_Worlds_);

			BatchedMeshes_.reserve(MaxNum_Batches_);

			auto config{ Utils::LoadFromFile<NLohmannJSON>("Assets/Configs/MeshCommon.json") };
			auto&& rsSetup{ DX12::LoadRootSignatureSetup(config.at("RS")) };
			RS_.Initialize(device, rsSetup);

			/*DX12Context_->Compile(
				VS_,
				L"Assets/Shaders/Model.VS.hlsl",
				L"vs_6_6",
				L"main",
				"Model.VS"
			);
			DX12Context_->Compile(
				PS_,
				L"Assets/Shaders/Model.PS.hlsl",
				L"ps_6_6",
				L"main",
				"Model.PS"
			);

			auto&& rasterizerState{ DX12::LoadRasterizerState(config.at("Model.PSO")) };
			auto&& depthStencilState{ DX12::LoadDepthStencilState(config.at("Model.PSO")) };
			auto&& inputLayout{ DX12::LoadInputLayout(config.at("Model.PSO")) };

			PSOs_.resize(static_cast<uint32_t>(BlendMode::Count));
			for (uint32_t i{ 0U }; i < static_cast<uint32_t>(BlendMode::Count); ++i) {
				auto&& arr_BlendState{ config.at("BlendStates").at(i) };
				BlendModeNames_.emplace_back(arr_BlendState.at(0).at("Name"));
				auto&& blendState{ DX12::LoadBlendState0(arr_BlendState) };

				PSOs_[i].reset(new DX12::GraphicsPSO{});
				PSOs_[i]->Initialize(
					device,
					RS_,
					VS_,
					PS_,
					blendState,
					rasterizerState,
					depthStencilState,
					inputLayout,
					D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
					DX12::GraphicsPSO::DefaultRTVFormats,
					DX12::GraphicsPSO::DefaultDSVFormat
				);
			}*/

			std::vector<D3D12_INDIRECT_ARGUMENT_DESC> argDescs{};
			auto& arg0{ argDescs.emplace_back(D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT) };
			arg0.Constant.RootParameterIndex = static_cast<uint32_t>(RootSignatureEntry::Constant_BatchIndex);
			arg0.Constant.DestOffsetIn32BitValues = 0U;
			arg0.Constant.Num32BitValuesToSet = 1U;
			//argDescs.emplace_back(D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW);
			argDescs.emplace_back(D3D12_INDIRECT_ARGUMENT_TYPE_DRAW);

			D3D12_COMMAND_SIGNATURE_DESC cmdSignatureDesc{
				.ByteStride{ sizeof(CommandArgs) },
				.NumArgumentDescs{ static_cast<uint32_t>(argDescs.size()) },
				.pArgumentDescs{ argDescs.data() }
			};
			DX12Context_->Device()->CreateCommandSignature(
				&cmdSignatureDesc,
				RS_.Get(),
				IID_PPV_ARGS(&CommandSignature_)
			);

			UB_CommandArgs_.Initialize(
				DX12Context_->Device(),
				sizeof(CommandArgs) * MaxNum_Batches_
			);

			DB_BatchedVertices_.Initialize(
				DX12Context_->Device(),
				sizeof(Utils::Mesh::Vertex) * MaxNum_BatchedVertices_
			);
			VBV_BatchedVertices_ = DX12::VBV::Create<Utils::Mesh::Vertex>(DB_BatchedVertices_);
		}

		void Begin(DX12::CommandList const& cmdList_) {
			CommandList_ = cmdList_.Get();
			CommandList_->SetGraphicsRootSignature(RS_.Get());
			Count_UnuploadedBatches_ = 0U;
			Count_BatchedVertices_ = 0U;
		}

		void End() {
			Count_UnuploadedBatches_ = 0U;
			Count_BatchedVertices_ = 0U;

			Array_CommandArgs_.clear();
		}

		void BatchBegin() {
			D3D12_RESOURCE_BARRIER const barrier{
				DX12::Barrier::Transition(
					DB_Worlds_,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
					D3D12_RESOURCE_STATE_COPY_DEST
				)
			};
			CommandList_->ResourceBarrier(1U, &barrier);
		}

		void Batch(
			MeshShaderAsset const& mesh_,
			uint32_t num_Instances_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_Material_,
			Mat4 const& world_
		) {
			auto const& device{ DX12Context_->Device() };

			uint32_t const idx{ Count_UnuploadedBatches_ };

			auto const& meshSRVs{ mesh_.LocalDescriptors() };
			device->CopyDescriptorsSimple(
				1U,
				Table_PositionArrays_.CPUHandle(idx),
				meshSRVs.CPUHandle(0U),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
			device->CopyDescriptorsSimple(
				1U,
				Table_TexCoordArrays_.CPUHandle(idx),
				meshSRVs.CPUHandle(1U),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
			device->CopyDescriptorsSimple(
				1U,
				Table_NormalArrays_.CPUHandle(idx),
				meshSRVs.CPUHandle(2U),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);
			device->CopyDescriptorsSimple(
				1U,
				Table_TangentArrays_.CPUHandle(idx),
				meshSRVs.CPUHandle(3U),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);

			device->CopyDescriptorsSimple(
				1U,
				Table_Materials_.CPUHandle(idx),
				localCBV_Material_,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);

			UB_Worlds_.Store(
				&world_,
				sizeof(Mat4),
				sizeof(Mat4) * (idx)
			);

			auto& cmdArgs{ Array_CommandArgs_.emplace_back() };
			cmdArgs.BatchIndex = idx;
			cmdArgs.DrawArgs = D3D12_DRAW_ARGUMENTS{
				mesh_.Num_Vertices(),
				num_Instances_,
				Count_BatchedVertices_,
				0U,
			};

			CommandList_->CopyBufferRegion(
				DB_BatchedVertices_.Get(),
				sizeof(Utils::Mesh::Vertex) * Count_BatchedVertices_,
				mesh_.VertexBuffer().Get(),
				0LLU,
				mesh_.VertexBuffer().SizeInBytes()
			);

			BatchedMeshes_.emplace_back(&mesh_);

			++Count_UnuploadedBatches_;
			Count_BatchedVertices_ += mesh_.Num_Vertices();
		}

		void BatchEnd() {
			CommandList_->CopyBufferRegion(
				DB_Worlds_.Get(),
				0LLU,
				UB_Worlds_.Get(),
				sizeof(Mat4) * Count_UploadedBatches_,
				sizeof(Mat4) * Count_UnuploadedBatches_
			);
		}

		void Render(
			DX12::GraphicsPipelineState const& pso_,
			D3D12_GPU_DESCRIPTOR_HANDLE globalSRV_TextureStart_,
			D3D12_GPU_DESCRIPTOR_HANDLE globalCBV_Lighting_,
			D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_
		) {
			DX12Context_->Device()->CopyDescriptorsSimple(
				1U,
				Table_VP_And_Worlds_.CPUHandle(0U),
				localCBV_VP_,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);

			D3D12_RESOURCE_BARRIER const barrier{
				DX12::Barrier::Transition(
					DB_Worlds_,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
				)
			};
			D3D12_RESOURCE_BARRIER const barrier1{
				DX12::Barrier::Transition(
					DB_BatchedVertices_,
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
				)
			};
			CommandList_->ResourceBarrier(1U, &barrier);
			CommandList_->ResourceBarrier(1U, &barrier1);

			CommandList_->SetPipelineState(pso_.Get());
			CommandList_->SetGraphicsRootDescriptorTable(
				static_cast<uint32_t>(RootSignatureEntry::Table_VertexData),
				Table_PositionArrays_.GPUHandle(0U)
			);
			CommandList_->SetGraphicsRootDescriptorTable(
				static_cast<uint32_t>(RootSignatureEntry::Table_VP_And_Worlds),
				Table_VP_And_Worlds_.GPUHandle(0U)
			);
			CommandList_->SetGraphicsRootDescriptorTable(
				static_cast<uint32_t>(RootSignatureEntry::Table_Materials),
				Table_Materials_.GPUHandle(0U)
			);
			CommandList_->SetGraphicsRootDescriptorTable(
				static_cast<uint32_t>(RootSignatureEntry::Table_Textures),
				globalSRV_TextureStart_
			);
			CommandList_->SetGraphicsRootDescriptorTable(
				static_cast<uint32_t>(RootSignatureEntry::Table_Lighting),
				globalCBV_Lighting_
			);

			CommandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			CommandList_->IASetVertexBuffers(0U, 1U, &VBV_BatchedVertices_);

			UB_CommandArgs_.Store(
				Array_CommandArgs_.data(),
				sizeof(CommandArgs) * Count_UnuploadedBatches_,
				sizeof(CommandArgs) * Count_UploadedBatches_
			);
			if (Count_UnuploadedBatches_) {
				CommandList_->ExecuteIndirect(
					CommandSignature_,
					Count_UnuploadedBatches_,
					UB_CommandArgs_.Get(),
					sizeof(CommandArgs) * Count_UploadedBatches_,
					nullptr,
					0LLU
				);
			}

			D3D12_RESOURCE_BARRIER const barrier3{
				DX12::Barrier::Transition(
					DB_BatchedVertices_,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
					D3D12_RESOURCE_STATE_COPY_DEST
				)
			};
			CommandList_->ResourceBarrier(1U, &barrier3);
		}

	public:
		MeshManager(){}

		~MeshManager() {
			CommandSignature_->Release();
		}

	private:
		DX12::RootSignature RS_{};

		DX12::DescriptorTable Table_Materials_{};
		DX12::DescriptorTable Table_PositionArrays_{};
		DX12::DescriptorTable Table_TexCoordArrays_{};
		DX12::DescriptorTable Table_NormalArrays_{};
		DX12::DescriptorTable Table_TangentArrays_{};
		DX12::DescriptorTable Table_VP_And_Worlds_{};
		DX12::DefaultBuffer DB_Worlds_{};
		DX12::UploadBuffer UB_Worlds_{};
		DX12::DefaultBuffer DB_VP_{};
		DX12::UploadBuffer UB_VP_{};

		DX12::DefaultBuffer DB_BatchedVertices_{};
		DX12::VBV VBV_BatchedVertices_{};

		ID3D12CommandSignature* CommandSignature_{ nullptr };
		DX12::UploadBuffer UB_CommandArgs_{};

		std::vector<MeshShaderAsset const*> BatchedMeshes_{};

		uint32_t Count_UnuploadedBatches_{ 0U };
		uint32_t Count_UploadedBatches_{ 0U };
		uint32_t Count_BatchedVertices_{ 0U };

		DX12::Context const* DX12Context_{ nullptr };
		ID3D12GraphicsCommandList* CommandList_{ nullptr };

		uint32_t MaxNum_Batches_{ 2048U };
		uint32_t MaxNum_BatchedVertices_{ 65536U };

		enum class RootSignatureEntry : uint32_t {
			Constant_BatchIndex = 0U,
			Table_VertexData = 1U,
			Table_VP_And_Worlds = 2U,
			Table_Materials = 3U,
			Table_Textures = 4U,
			Table_Lighting = 5U,
		};

		struct CommandArgs {
			uint32_t BatchIndex;
			D3D12_DRAW_ARGUMENTS DrawArgs;
		};
		std::vector<CommandArgs> Array_CommandArgs_{};
	};
}