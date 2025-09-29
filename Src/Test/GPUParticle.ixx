module;

#include<External/nlohmann.JSON/single_include/nlohmann/json.hpp>

#include<External/ImGui/imgui.h>

export module Lumina.GPUParticle;

import <cstdint>;

import <memory>;

import <fstream>;

import <d3d12.h>;

import Lumina.DX12;
import Lumina.DX12.Aux;

import Lumina.Utils.Data;

export namespace Lumina::Test {
	struct Particle {
		float Position[3];
		float Velocity[3];
		float Color[4];
		uint32_t Duration;
		uint32_t FrameCnt;
	};

	struct Vertex {
		float Position[4];
		float TexCoord[2];
	};

	struct EnvironmentVariables {
		float Time;
		float AccelFieldCoefs[9];
	};

	class ParticleManager final {
	public:
		void BatchEmission(uint32_t num_, const Particle& initData_);
		void DispatchEmissions(
			DX12::CommandQueue& computeQueue_,
			const DX12::DescriptorHeap& cbvsrvuavHeap_
		);

		void Update(
			DX12::CommandQueue& computeQueue_,
			const DX12::DescriptorHeap& cbvsrvuavHeap_
		);
		void Render(
			DX12::CommandList& directList_,
			const DX12::DescriptorHeap& cbvsrvuavHeap_,
			const DX12::DescriptorTable& vpCBVTable_,
			const DX12::DescriptorTable& texSRVTable_
		);

	public:
		void Initialize(
			const DX12::GraphicsDevice& device_,
			DX12::CommandQueue& directQueue_,
			DX12::CommandQueue& computeQueue_,
			const DX12::DescriptorHeap& cbvsrvuavHeap_,
			const DX12::Shader::Compiler& shaderCompiler_
		);

	public:
		ParticleManager() = default;
		~ParticleManager() = default;

	private:
		DX12::UnorderedAccessBuffer ParticlePool_{};
		DX12::UnorderedAccessBuffer Indices_Active_{};
		DX12::UnorderedAccessBuffer ActiveCounter_{};
		DX12::UploadBuffer ActiveCounterReset_{};
		DX12::ReadbackBuffer ActiveCounterReadback_{};
		DX12::UnorderedAccessBuffer Indices_Inactive_{};
		DX12::UnorderedAccessBuffer InactiveCounter_{};
		DX12::ReadbackBuffer InactiveCounterReadback_{};
		DX12::UploadBuffer InitDataArray_{};
		DX12::UploadBuffer EnvironmentVariables_{};
		EnvironmentVariables EnvVars_{};
		uint32_t Num_Emitted_{ 0U };

		DX12::CommandAllocator ComputeAllocator_{};
		DX12::CommandList ComputeList_{};
		DX12::CommandAllocator DirectAllocator_{};
		DX12::CommandList DirectList_{};

		DX12::RootSignature RS_{};
		DX12::Shader InitializeShader_{};
		DX12::ComputePSO InitializePSO_{};
		DX12::Shader EmitShader_{};
		DX12::ComputePSO EmitPSO_{};
		DX12::Shader UpdateShader_{};
		DX12::ComputePSO UpdatePSO_{};
		
		DX12::RootSignature RenderRS_{};
		DX12::Shader RenderVertexShader_{};
		DX12::Shader RenderPixelShader_{};
		DX12::GraphicsPSO RenderPSO_{};

		DX12::DescriptorTable Table_{};
		enum class DescriptorList : uint32_t {
			UAV_ParticlePool,
			UAV_In_Indices_Active,
			UAV_In_Indices_Inactive,
			UAV_Out_Indices_Inactive,
			SRV_ParticleInitDataArray,
			CBV_EnvVars,
			SRV_ParticlePool,
			SRV_Indices_Active,
		};

		const uint32_t MaxNum_Particles_{ 1U << 22U };
		const uint32_t MaxNum_Emitted_{ 1U << 12U };
		const uint32_t Log2_Num_ThreadX_{ 8U };

		DX12::DefaultBuffer QuadVertexBuffer_{};
		DX12::DefaultBuffer QuadIndexBuffer_{};
		D3D12_VERTEX_BUFFER_VIEW QuadVBV_{};
		D3D12_INDEX_BUFFER_VIEW QuadIBV_{};
	};

	void ParticleManager::BatchEmission(uint32_t num_, const Particle& initData_) {
		uint32_t currentInactiveNum{};
		InactiveCounterReadback_.Load(&currentInactiveNum, sizeof(uint32_t), 0LLU);
		if (Num_Emitted_ + num_ <= std::min<uint32_t>(MaxNum_Emitted_, currentInactiveNum)) {
			for (uint32_t i{ 0U }; i < num_; ++i) {
				InitDataArray_.Store(&initData_, sizeof(Particle), sizeof(Particle) * Num_Emitted_ + i);
			}
			Num_Emitted_ += num_;
		}
	}

	// For now Num_Emitted_ must be a multiple of (1U << Log2_Num_ThreadX_).
	void ParticleManager::DispatchEmissions(
		DX12::CommandQueue& computeQueue_,
		const DX12::DescriptorHeap& cbvsrvuavHeap_
	) {
		if ((Num_Emitted_ >> Log2_Num_ThreadX_) == 0U) { return; }

		ID3D12DescriptorHeap* descriptorHeaps[]{ cbvsrvuavHeap_.Get() };
		ComputeList_->SetDescriptorHeaps(1U, descriptorHeaps);
		ComputeList_->SetPipelineState(EmitPSO_.Get());
		ComputeList_->SetComputeRootSignature(RS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, Table_.GPUHandle(0U));
		ComputeList_->Dispatch(Num_Emitted_ >> Log2_Num_ThreadX_, 1U, 1U);
		
		computeQueue_ << ComputeList_;

		auto fenceVal{ computeQueue_.ExecuteBatchedCommandLists() };
		computeQueue_.CPUWait(fenceVal);
		ComputeList_.Reset(ComputeAllocator_);
		Num_Emitted_ = 0U;
	}

	void ParticleManager::Update(
		DX12::CommandQueue& computeQueue_,
		const DX12::DescriptorHeap& cbvsrvuavHeap_
	) {
		static const D3D12_RESOURCE_BARRIER barriers[6]{
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Transition{
					.pResource{ ActiveCounter_.Get() },
					.StateBefore{ D3D12_RESOURCE_STATE_COPY_SOURCE },
					.StateAfter{ D3D12_RESOURCE_STATE_COPY_DEST },
				},
			},
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Transition{
					.pResource{ InactiveCounter_.Get() },
					.StateBefore{ D3D12_RESOURCE_STATE_COPY_SOURCE },
					.StateAfter{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
				},
			},
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Transition{
					.pResource{ ActiveCounter_.Get() },
					.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
					.StateAfter{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
				},
			},
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Transition{
					.pResource{ ActiveCounter_.Get() },
					.StateBefore{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
					.StateAfter{ D3D12_RESOURCE_STATE_COPY_SOURCE },
				},
			},
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Transition{
					.pResource{ InactiveCounter_.Get() },
					.StateBefore{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
					.StateAfter{ D3D12_RESOURCE_STATE_COPY_SOURCE },
				},
			},
			{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
				.UAV{.pResource{ nullptr } },
			},
		};

		EnvVars_.Time += 0.01f;
		EnvVars_.AccelFieldCoefs[0] = -std::cos(EnvVars_.Time * 0.2f);
		EnvVars_.AccelFieldCoefs[1] = -std::cos(EnvVars_.Time * 0.8f);
		EnvVars_.AccelFieldCoefs[2] = std::cos(EnvVars_.Time * 0.6f);
		EnvVars_.AccelFieldCoefs[3] = std::cos(EnvVars_.Time * 0.7f);
		EnvVars_.AccelFieldCoefs[4] = -std::cos(EnvVars_.Time * 0.1f);
		EnvVars_.AccelFieldCoefs[5] = -std::cos(EnvVars_.Time * 0.9f);
		EnvVars_.AccelFieldCoefs[6] = -std::cos(EnvVars_.Time * 0.3f);
		EnvVars_.AccelFieldCoefs[7] = std::cos(EnvVars_.Time * 0.4f);
		EnvVars_.AccelFieldCoefs[8] = -std::cos(EnvVars_.Time * 0.5f);
		std::memcpy(EnvironmentVariables_(), &EnvVars_, sizeof(EnvironmentVariables));

		ID3D12DescriptorHeap* descriptorHeaps[]{ cbvsrvuavHeap_.Get() };
		ComputeList_->SetDescriptorHeaps(1U, descriptorHeaps);
		ComputeList_->SetPipelineState(UpdatePSO_.Get());
		ComputeList_->SetComputeRootSignature(RS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, Table_.GPUHandle(DescriptorList::UAV_ParticlePool));

		ComputeList_->ResourceBarrier(2U, barriers + 0U);
		ComputeList_->CopyResource(ActiveCounter_.Get(), ActiveCounterReset_.Get());
		ComputeList_->ResourceBarrier(1U, barriers + 2U);
		ComputeList_->Dispatch(MaxNum_Particles_ >> Log2_Num_ThreadX_, 1U, 1U);
		ComputeList_->ResourceBarrier(3U, barriers + 3U);
		ComputeList_->CopyResource(ActiveCounterReadback_.Get(), ActiveCounter_.Get());
		ComputeList_->CopyResource(InactiveCounterReadback_.Get(), InactiveCounter_.Get());
		computeQueue_ << ComputeList_;
		computeQueue_.CPUWait(computeQueue_.ExecuteBatchedCommandLists());
		ComputeList_.Reset(ComputeAllocator_);
	}

	void ParticleManager::Render(
		DX12::CommandList& directList_,
		const DX12::DescriptorHeap& cbvsrvuavHeap_,
		const DX12::DescriptorTable& vpCBVTable_,
		const DX12::DescriptorTable& texSRVTable_
	) {
		ID3D12DescriptorHeap* descriptorHeaps[]{ cbvsrvuavHeap_.Get() };
		directList_->SetDescriptorHeaps(1U, descriptorHeaps);
		directList_->SetPipelineState(RenderPSO_.Get());
		directList_->SetGraphicsRootSignature(RenderRS_.Get());
		directList_->SetGraphicsRootDescriptorTable(0U, Table_.GPUHandle(DescriptorList::SRV_ParticlePool));
		directList_->SetGraphicsRootDescriptorTable(1U, vpCBVTable_.GPUHandle(0U));
		directList_->SetGraphicsRootDescriptorTable(2U, texSRVTable_.GPUHandle(0U));
		directList_->IASetVertexBuffers(0U, 1U, &QuadVBV_);
		directList_->IASetIndexBuffer(&QuadIBV_);
		directList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		uint32_t currentActiveNum{};
		ActiveCounterReadback_.Load(&currentActiveNum, sizeof(uint32_t), 0LLU);
		directList_->DrawIndexedInstanced(6U, currentActiveNum, 0U, 0U, 0U);

		ImGui::Begin("GPU Particle");
		ImGui::Text("#(Active) = %d", currentActiveNum);
		ImGui::End();
	}

	void ParticleManager::Initialize(
		const DX12::GraphicsDevice& device_,
		DX12::CommandQueue& directQueue_,
		DX12::CommandQueue& computeQueue_,
		const DX12::DescriptorHeap& cbvsrvuavHeap_,
		const DX12::Shader::Compiler& shaderCompiler_
	) {
		ComputeAllocator_.Initialize(device_, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		ComputeList_.Initialize(device_, ComputeAllocator_);
		DirectAllocator_.Initialize(device_, D3D12_COMMAND_LIST_TYPE_DIRECT);
		DirectList_.Initialize(device_, DirectAllocator_);

		Table_ = cbvsrvuavHeap_.Allocate(8U);

		ParticlePool_.Initialize(device_, sizeof(Particle) * MaxNum_Particles_);
		DX12::UAV<Particle>::Create(
			device_,
			Table_.CPUHandle(DescriptorList::UAV_ParticlePool),
			ParticlePool_
		);
		DX12::SRV<Particle>::Create(
			device_,
			Table_.CPUHandle(DescriptorList::SRV_ParticlePool),
			ParticlePool_
		);

		Indices_Active_.Initialize(device_, sizeof(uint32_t) * MaxNum_Particles_);
		ActiveCounter_.Initialize(device_, sizeof(uint32_t) * 1LLU);
		ActiveCounterReset_.Initialize(device_, sizeof(uint32_t) * 1LLU);
		const uint32_t zero{ 0U };
		ActiveCounterReset_.Store(&zero, sizeof(uint32_t), 0LLU);
		ActiveCounterReadback_.Initialize(device_, sizeof(uint32_t) * 1LLU);
		DX12::UAV<uint32_t>::Create(
			device_,
			Table_.CPUHandle(DescriptorList::UAV_In_Indices_Active),
			Indices_Active_,
			ActiveCounter_
		);
		DX12::SRV<uint32_t>::Create(
			device_,
			Table_.CPUHandle(DescriptorList::SRV_Indices_Active),
			Indices_Active_
		);

		Indices_Inactive_.Initialize(device_, sizeof(uint32_t) * MaxNum_Particles_);
		InactiveCounter_.Initialize(device_, sizeof(uint32_t) * 1LLU);
		InactiveCounterReadback_.Initialize(device_, sizeof(uint32_t) * 1LLU);
		DX12::UAV<uint32_t>::Create(
			device_,
			Table_.CPUHandle(DescriptorList::UAV_In_Indices_Inactive),
			Indices_Inactive_,
			InactiveCounter_
		);
		DX12::UAV<uint32_t>::Create(
			device_,
			Table_.CPUHandle(DescriptorList::UAV_Out_Indices_Inactive),
			Indices_Inactive_,
			InactiveCounter_
		);

		InitDataArray_.Initialize(device_, sizeof(Particle) * MaxNum_Emitted_);
		DX12::SRV<Particle>::Create(
			device_,
			Table_.CPUHandle(DescriptorList::SRV_ParticleInitDataArray),
			InitDataArray_
		);

		EnvironmentVariables_.Initialize(device_, (sizeof(EnvironmentVariables) + 0xFF) & ~0xFF);
		DX12::CBV::Create(
			device_,
			Table_.CPUHandle(DescriptorList::CBV_EnvVars),
			EnvironmentVariables_
		);
		EnvVars_.Time = 0.0f;
		for (auto& accelFieldCoef : EnvVars_.AccelFieldCoefs) { accelFieldCoef = 0.0f; }

		NLohmannJSON config{ Utils::LoadFromFile<NLohmannJSON>("Assets/Configs/GPUParticleConfig.json") };
		auto&& rsSetup{ DX12::LoadRootSignatureSetup(config.at("GPUParticle RS")) };
		RS_.Initialize(device_, rsSetup);

		InitializeShader_.Initialize(
			shaderCompiler_,
			L"ParticleCS.hlsl",
			L"cs_6_6",
			L"Initialize"
		);
		InitializePSO_.Initialize(device_, RS_, InitializeShader_);

		EmitShader_.Initialize(
			shaderCompiler_,
			L"ParticleCS.hlsl",
			L"cs_6_6",
			L"Emit"
		);
		EmitPSO_.Initialize(device_, RS_, EmitShader_);

		UpdateShader_.Initialize(
			shaderCompiler_,
			L"ParticleCS.hlsl",
			L"cs_6_6",
			L"Update"
		);
		UpdatePSO_.Initialize(device_, RS_, UpdateShader_);

		ID3D12DescriptorHeap* descriptorHeaps[]{ cbvsrvuavHeap_.Get() };
		ComputeList_->SetDescriptorHeaps(1U, descriptorHeaps);
		ComputeList_->SetPipelineState(InitializePSO_.Get());
		ComputeList_->SetComputeRootSignature(RS_.Get());
		ComputeList_->SetComputeRootDescriptorTable(0U, Table_.GPUHandle(0U));
		ComputeList_->Dispatch(MaxNum_Particles_ >> Log2_Num_ThreadX_, 1U, 1U);
		ComputeList_->CopyResource(ActiveCounterReadback_.Get(), ActiveCounter_.Get());
		ComputeList_->CopyResource(InactiveCounterReadback_.Get(), InactiveCounter_.Get());
		computeQueue_ << ComputeList_;
		computeQueue_.CPUWait(computeQueue_.ExecuteBatchedCommandLists());
		ComputeList_.Reset(ComputeAllocator_);

		//----	------	------	------	------	----//

		auto&& renderRSSetup{ DX12::LoadRootSignatureSetup(config.at("GPUParticle Render RS")) };
		RenderRS_.Initialize(device_, renderRSSetup);
		RenderVertexShader_.Initialize(
			shaderCompiler_,
			L"Assets/ShaderTest/ParticleVS.hlsl",
			L"vs_6_6",
			L"main"
		);
		RenderPixelShader_.Initialize(
			shaderCompiler_,
			L"Assets/ShaderTest/ParticlePS.hlsl",
			L"ps_6_6",
			L"main"
		);
		DX12::BlendState blendState{};
		{
			blendState.AlphaToCoverageEnable = false;
			blendState.IndependentBlendEnable = false;
			blendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
				.BlendEnable{ true },
				.LogicOpEnable{ false },
				.SrcBlend{ D3D12_BLEND_SRC_ALPHA },
				.DestBlend{ D3D12_BLEND_ONE },
				.BlendOp{ D3D12_BLEND_OP_ADD },
				.SrcBlendAlpha{ D3D12_BLEND_SRC_ALPHA },
				.DestBlendAlpha{ D3D12_BLEND_ONE },
				.BlendOpAlpha{ D3D12_BLEND_OP_ADD },
				.LogicOp{ D3D12_LOGIC_OP_NOOP },
				.RenderTargetWriteMask{ D3D12_COLOR_WRITE_ENABLE_ALL },
			};
		}
		auto&& rasterizerState{ DX12::LoadRasterizerState(config.at("GPUParticle Render PSO")) };
		DX12::DepthStencilState depthStencilState{
			.DepthEnable{ false },
			//.DepthWriteMask{ D3D12_DEPTH_WRITE_MASK_ALL },
			//.DepthFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
		};
		auto&& inputLayout{ DX12::LoadInputLayout(config.at("GPUParticle Render PSO")) };
		RenderPSO_.Initialize(
			device_,
			RenderRS_,
			RenderVertexShader_,
			RenderPixelShader_,
			blendState,
			rasterizerState,
			depthStencilState,
			inputLayout,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			DX12::GraphicsPSO::DefaultRTVFormats,
			DX12::GraphicsPSO::DefaultDSVFormat
		);

		//----	------	------	------	------	----//

		QuadVertexBuffer_.Initialize(device_, sizeof(Vertex) * 4U);
		QuadVBV_ = DX12::VBV<Vertex>{ QuadVertexBuffer_ };
		float halfSide{ 1.0f / 32.0f };
		Vertex quadVerts[4]{
			{ .Position{ -halfSide, halfSide, 0.0f, 1.0f }, .TexCoord{ 0.0f, 0.0f }, },
			{ .Position{ halfSide, halfSide, 0.0f, 1.0f }, .TexCoord{ 1.0f, 0.0f }, },
			{ .Position{ -halfSide, -halfSide, 0.0f, 1.0f }, .TexCoord{ 0.0f, 1.0f }, },
			{ .Position{ halfSide, -halfSide, 0.0f, 1.0f }, .TexCoord{ 1.0f, 1.0f }, },
		};
		DX12::UploadBuffer vbTmp{};
		vbTmp.Initialize(device_, sizeof(Vertex) * 4U);
		vbTmp.Store(quadVerts, QuadVertexBuffer_.SizeInBytes(), 0LLU);

		QuadIndexBuffer_.Initialize(device_, sizeof(uint32_t) * 6U);
		QuadIBV_ = DX12::IBV{ QuadIndexBuffer_ };
		uint32_t quadIndices[6]{ 0U, 1U, 2U, 1U, 3U, 2U, };
		DX12::UploadBuffer ibTmp{};
		ibTmp.Initialize(device_, sizeof(uint32_t) * 6U);
		ibTmp.Store(quadIndices, QuadIndexBuffer_.SizeInBytes(), 0LLU);

		DirectList_->CopyResource(QuadVertexBuffer_.Get(), vbTmp.Get());
		DirectList_->CopyResource(QuadIndexBuffer_.Get(), ibTmp.Get());
		directQueue_ << DirectList_;
		directQueue_.CPUWait(directQueue_.ExecuteBatchedCommandLists());
		DirectList_.Reset(DirectAllocator_);
	}

	/*ParticleManager::ParticleManager(const DX12::GraphicsDevice& device_) {
	}*/
}