export module ParticleSystem;

import <cstdint>;
import <type_traits>;

import Lumina.Container.List;

import Lumina.Math.Numerics;
import Lumina.Math.Vector;
import Lumina.Math.Matrix;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux;
import Lumina.DX12.Aux.View;

import Lumina.MeshManager;

namespace {
	inline void StageRenderCommands(
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::DX12::RootSignature const& rs_,
		Lumina::DX12::GraphicsPSO const& graphicsPSO_,
		D3D12_GPU_DESCRIPTOR_HANDLE srv_ParticleRenderData_,
		D3D12_GPU_DESCRIPTOR_HANDLE cbv_SceneVars_,
		D3D12_GPU_DESCRIPTOR_HANDLE cbv_VP_,
		Lumina::DX12::DescriptorTable const& table_Textures_,
		Lumina::DX12::DescriptorTable const& table_Textures2_,
		D3D12_VERTEX_BUFFER_VIEW const& particleMeshVBV_,
		D3D12_INDEX_BUFFER_VIEW const& particleMeshIBV_,
		uint32_t num_Inst_
	) {
		cmdList_->SetGraphicsRootSignature(rs_.Get());
		cmdList_->SetPipelineState(graphicsPSO_.Get());
		// Array of render data
		cmdList_->SetGraphicsRootDescriptorTable(0U, srv_ParticleRenderData_);
		cmdList_->SetGraphicsRootDescriptorTable(1U, cbv_SceneVars_);
		cmdList_->SetGraphicsRootDescriptorTable(2U, cbv_VP_);
		cmdList_->SetGraphicsRootDescriptorTable(3U, table_Textures_.GPUHandle(0U));
		cmdList_->SetGraphicsRootDescriptorTable(4U, table_Textures2_.GPUHandle(0U));
		cmdList_->IASetVertexBuffers(0U, 1U, &particleMeshVBV_);
		cmdList_->IASetIndexBuffer(&particleMeshIBV_);
		cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		if (num_Inst_ > 0U) {
			cmdList_->DrawIndexedInstanced(6U, num_Inst_, 0U, 0U, 0U);
		}
	}

	struct ParticleSpriteVertex {
		Lumina::Float4 Position;
		Lumina::Float2 TexCoord;
	};
}

export struct Particle {
	Lumina::Float3 Scale{ 1.0f, 1.0f, 1.0f };
	Lumina::Float3 Rotate{ 0.0f, 0.0f, 0.0f };
	Lumina::Float3 Translate{ 0.0f, 0.0f, 0.0f };

	Lumina::Float3 Velocity;

	float Life{ 10.0f };

	struct RenderDataCollection {
		Lumina::Float4 RGBA{ 1.0f, 1.0f, 1.0f, 1.0f };
		uint32_t DiffuseID;
		uint32_t DiffuseAtlasID;
	};

	RenderDataCollection RenderData;
};

template<typename T>
concept Concept_Particle = std::is_base_of_v<Particle, T>;

export template<Concept_Particle T>
class ParticleSystem {
	using ParticleUpdateFunc = bool(*)(T&, void const*);

public:
	typename Lumina::List<T> const& InstanceList() const noexcept;

public:
	/// <summary>
	/// Batches a particle.
	/// </summary>
	/// <param name="p_">Particle</param>
	void Emit(T&& p_) {
		if (!Instances_.IsFull()) {
			auto& p{ Instances_.New() };
			p = std::move(p_);
		}
	}
	/// <summary>
	/// Erase all alive particles.
	/// </summary>
	void Clear() {
		Instances_.Clear();
		Count_Alive_ = 0U;
	}

public:
	/// <summary>
	/// Updates alive particles.
	/// </summary>
	/// <param name="cmdList_">Command list</param>
	/// <param name="viewToWorld_">View-to-world matrix used for billboard</param>
	/// <param name="updateFunc_">Callback function in which a particle is updated</param>
	/// <param name="updateFuncParam_">Callback function parameter</param>
	void Update(
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::Mat4 const& viewToWorld_,
		ParticleUpdateFunc updateFunc_ = nullptr,
		void const* updateFuncParam_ = nullptr
	);

	/// <summary>
	/// Renders alive particles.
	/// </summary>
	/// <param name="cmdList_">Command list</param>
	/// <param name="rs_">Root signature</param>
	/// <param name="graphicsPSO_">PSO</param>
	/// <param name="localCBV_SceneVars_">Shader-invisible CBV for scene constants</param>
	/// <param name="localCBV_VP_">Shader-invisible CBV for world-to-NDC matrix</param>
	/// <param name="globalTable_Textures_">Shader-visible image texture SRV table</param>
	/// <param name="globalTable_Textures2_">Shader-visible G-buffer SRV table</param>
	void Render(
		Lumina::DX12::CommandList const& cmdList_,
		Lumina::DX12::RootSignature const& rs_,
		Lumina::DX12::GraphicsPSO const& graphicsPSO_,
		D3D12_CPU_DESCRIPTOR_HANDLE localCBV_SceneVars_,
		D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_,
		Lumina::DX12::DescriptorTable const& globalTable_Textures_,
		Lumina::DX12::DescriptorTable const& globalTable_Textures2_
	);

public:
	void Initialize(
		Lumina::DX12::Context const& dx12Context_,
		uint32_t num_
	);

private:
	Lumina::List<T> Instances_{};
	uint32_t Count_Alive_{ 0U };

private:
	Lumina::DX12::Context const* DX12Context_{ nullptr };

	Lumina::DX12::DefaultBuffer DB_Array_RenderData_{};
	Lumina::DX12::UploadBuffer UB_Array_RenderData_{};

	Lumina::DX12::DescriptorTable GlobalTable_{};

	Lumina::DX12::DefaultBuffer QuadVertexBuffer_{};
	Lumina::DX12::DefaultBuffer QuadIndexBuffer_{};
	D3D12_VERTEX_BUFFER_VIEW QuadVBV_{};
	D3D12_INDEX_BUFFER_VIEW QuadIBV_{};
};

template<Concept_Particle T>
typename Lumina::List<T> const& ParticleSystem<T>::InstanceList() const noexcept {
	return Instances_;
}

template<Concept_Particle T>
void ParticleSystem<T>::Update(
	Lumina::DX12::CommandList const& cmdList_,
	Lumina::Mat4 const& viewToWorld_,
	ParticleUpdateFunc updateFunc_,
	void const* updateFuncParam_
) {
	Count_Alive_ = 0U;

	typename decltype(Instances_)::Iterator it{ Instances_ };
	for (it.Begin(); !it.End(); it.Next()) {
		auto& particle{ (*it) };
		int32_t isAlive{ 1 };

		if (particle.Life <= 0.0f) {
			Instances_.Delete(it);
			isAlive = 0;
		}

		if (updateFunc_ != nullptr) {
			if (!updateFunc_(particle, updateFuncParam_)) {
				Instances_.Delete(it);
				isAlive = 0;
			}
		}
		else {
			particle.Translate.x += particle.Velocity.x;
			particle.Translate.y += particle.Velocity.y;
			particle.Translate.z += particle.Velocity.z;
			particle.Life -= 1.0f;
		}
		
		if (isAlive) {
			auto&& transform{
				Lumina::Mat4::SRT(
					Lumina::Vec3{ &particle.Scale.x },
					Lumina::Vec3{ &particle.Rotate.x },
					Lumina::Vec3{ &particle.Translate.x }
				)
			};

			Lumina::Mat4::Multiply(transform, transform, viewToWorld_);
			transform[3][0] = particle.Translate.x;
			transform[3][1] = particle.Translate.y;
			transform[3][2] = particle.Translate.z;

			// Transform
			UB_Array_RenderData_.Store(
				&transform,
				sizeof(Lumina::Mat4),
				(sizeof(Lumina::Mat4) + sizeof(T::RenderDataCollection)) * Count_Alive_ + 0LLU
			);
			// RGBA, TextureID, AtlasID
			UB_Array_RenderData_.Store(
				&particle.RenderData,
				sizeof(T::RenderDataCollection),
				(sizeof(Lumina::Mat4) + sizeof(T::RenderDataCollection)) * Count_Alive_ + sizeof(Lumina::Mat4)
			);

			++Count_Alive_;
		}
	}

	D3D12_RESOURCE_BARRIER const barriers_BeforeCopy[]{
		Lumina::DX12::Barrier::Transition(
			DB_Array_RenderData_,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_DEST
		),
	};
	D3D12_RESOURCE_BARRIER const barriers_AfterCopy[]{
		Lumina::DX12::Barrier::Transition(
			DB_Array_RenderData_,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
		),
	};
	cmdList_->ResourceBarrier(1U, barriers_BeforeCopy);
	cmdList_->CopyBufferRegion(
		DB_Array_RenderData_.Get(),
		0LLU,
		UB_Array_RenderData_.Get(),
		0LLU,
		(sizeof(Lumina::Mat4) + sizeof(T::RenderDataCollection)) * Count_Alive_
	);
	cmdList_->ResourceBarrier(1U, barriers_AfterCopy);
}

template<Concept_Particle T>
void ParticleSystem<T>::Render(
	Lumina::DX12::CommandList const& cmdList_,
	Lumina::DX12::RootSignature const& rs_,
	Lumina::DX12::GraphicsPSO const& graphicsPSO_,
	D3D12_CPU_DESCRIPTOR_HANDLE localCBV_SceneVars_,
	D3D12_CPU_DESCRIPTOR_HANDLE localCBV_VP_,
	Lumina::DX12::DescriptorTable const& globalTable_Textures_,
	Lumina::DX12::DescriptorTable const& globalTable_Textures2_
) {
	DX12Context_->Device()->CopyDescriptorsSimple(
		1U,
		GlobalTable_.CPUHandle(1U),
		localCBV_SceneVars_,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);
	DX12Context_->Device()->CopyDescriptorsSimple(
		1U,
		GlobalTable_.CPUHandle(2U),
		localCBV_VP_,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	StageRenderCommands(
		cmdList_,
		rs_,
		graphicsPSO_,
		GlobalTable_.GPUHandle(0U),
		GlobalTable_.GPUHandle(1U),
		GlobalTable_.GPUHandle(2U),
		globalTable_Textures_,
		globalTable_Textures2_,
		QuadVBV_,
		QuadIBV_,
		Count_Alive_
	);
}

template<Concept_Particle T>
void ParticleSystem<T>::Initialize(
	Lumina::DX12::Context const& dx12Context_,
	uint32_t num_
) {
	Instances_.Initialize(num_);

	DX12Context_ = &dx12Context_;
	auto const& device{ dx12Context_.Device() };

	struct RenderDataType {
		Lumina::Float4x4 Transform;
		typename T::RenderDataCollection RenderData;
	};
	DB_Array_RenderData_.Initialize(device, sizeof(RenderDataType) * num_);
	UB_Array_RenderData_.Initialize(device, DB_Array_RenderData_.SizeInBytes());

	dx12Context_.GlobalDescriptorHeap().Allocate(GlobalTable_, 3U);
	Lumina::DX12::SRV<RenderDataType>::Create(
		device,
		GlobalTable_.CPUHandle(0U),
		DB_Array_RenderData_
	);	

	QuadVertexBuffer_.Initialize(device, sizeof(ParticleSpriteVertex) * 4U);
	QuadVBV_ = Lumina::DX12::VBV::Create<ParticleSpriteVertex>(QuadVertexBuffer_);
	float halfSide{ 1.0f / 2.0f };
	ParticleSpriteVertex quadVerts[4]{
		{ .Position{ -halfSide, halfSide, 0.0f, 1.0f }, .TexCoord{ 0.0f, 0.0f }, },
		{ .Position{ halfSide, halfSide, 0.0f, 1.0f }, .TexCoord{ 1.0f, 0.0f }, },
		{ .Position{ -halfSide, -halfSide, 0.0f, 1.0f }, .TexCoord{ 0.0f, 1.0f }, },
		{ .Position{ halfSide, -halfSide, 0.0f, 1.0f }, .TexCoord{ 1.0f, 1.0f }, },
	};
	Lumina::DX12::UploadBuffer vbTmp{};
	vbTmp.Initialize(device, sizeof(ParticleSpriteVertex) * 4U);
	vbTmp.Store(quadVerts, QuadVertexBuffer_.SizeInBytes(), 0LLU);

	QuadIndexBuffer_.Initialize(device, sizeof(uint32_t) * 6U);
	QuadIBV_ = Lumina::DX12::IBV::Create(QuadIndexBuffer_);
	uint32_t quadIndices[6]{ 0U, 1U, 2U, 1U, 3U, 2U, };
	Lumina::DX12::UploadBuffer ibTmp{};
	ibTmp.Initialize(device, sizeof(uint32_t) * 6U);
	ibTmp.Store(quadIndices, QuadIndexBuffer_.SizeInBytes(), 0LLU);

	Lumina::DX12::CommandAllocator cmdAllocator{};
	cmdAllocator.Initialize(device);
	Lumina::DX12::CommandList cmdList{};
	cmdList.Initialize(device, cmdAllocator);
	cmdList->CopyResource(QuadVertexBuffer_.Get(), vbTmp.Get());
	cmdList->CopyResource(QuadIndexBuffer_.Get(), ibTmp.Get());
	dx12Context_.DirectQueue() << cmdList;
	dx12Context_.DirectQueue().CPUWait(dx12Context_.DirectQueue().ExecuteBatchedCommandLists());
}