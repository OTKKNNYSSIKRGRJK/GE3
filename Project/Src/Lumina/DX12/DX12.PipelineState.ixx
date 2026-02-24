module;

#include<d3d12.h>

#include<wrl.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : PipelineState;

//****	******	******	******	******	****//

import <cstdint>;

import <vector>;

import <string>;
import <format>;

import : GraphicsDevice;
import : Shader;
import : RootSignature;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

export namespace Lumina::DX12 {
	using BlendState = D3D12_BLEND_DESC;
	using RasterizerState = D3D12_RASTERIZER_DESC;
	using DepthStencilState = D3D12_DEPTH_STENCIL_DESC;
	using PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE;
	using ResourceFormat = DXGI_FORMAT;
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	GraphicsPipelineState					//
//	ComputePipelineState					//
//////	//////	//////	//////	//////	//////

export namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	GraphicsPipelineState					//
	//////	//////	//////	//////	//////	//////

	class GraphicsPipelineState final :
		public Wrapper<GraphicsPipelineState, ID3D12PipelineState>,
		public NonCopyable<GraphicsPipelineState> {
	public:
		class Setup;
		class InputLayout;

		//====	======	======	======	======	====//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			RootSignature const& rs_,
			Shader const& vertexShader_,
			Shader const& pixelShader_,
			BlendState const& blendState_,
			RasterizerState const& rasterizerState_,
			DepthStencilState const& depthStencilState_,
			InputLayout const& inputLayout_,
			PrimitiveTopologyType primitiveTopologyType_,
			std::vector<ResourceFormat> const& rtvFormats_,
			ResourceFormat dsvFormat_,
			std::string_view debugName_ = "GraphicsPSO"
		);
		void Initialize(
			GraphicsDevice const& device_,
			Setup const& psoSetup_,
			std::string_view debugName_ = "GraphicsPSO"
		);

		//----	------	------	------	------	----//

	public:
		constexpr GraphicsPipelineState() noexcept = default;
		constexpr virtual ~GraphicsPipelineState() noexcept = default;

		//====	======	======	======	======	====//

	public:
		static inline std::vector<ResourceFormat> DefaultRTVFormats{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, };
		static inline ResourceFormat DefaultDSVFormat{ DXGI_FORMAT_D24_UNORM_S8_UINT };
	};

	//====	======	======	======	======	====//

	using GraphicsPSO = GraphicsPipelineState;

	//////	//////	//////	//////	//////	//////
	//	ComputePipelineState					//
	//////	//////	//////	//////	//////	//////

	class ComputePipelineState final :
		public Wrapper<ComputePipelineState, ID3D12PipelineState>,
		public NonCopyable<ComputePipelineState> {
	public:
		void Initialize(
			GraphicsDevice const& device_,
			RootSignature const& rs_,
			Shader const& computeShader_,
			std::string_view debugName_ = "ComputePSO"
		);

		//----	------	------	------	------	----//

	public:
		constexpr ComputePipelineState() noexcept = default;
		constexpr ~ComputePipelineState() noexcept = default;
	};

	//====	======	======	======	======	====//

	using ComputePSO = ComputePipelineState;
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	class GraphicsPipelineState::Setup {
		friend GraphicsPipelineState;

	public:
		constexpr auto operator<<(RootSignature const& rootSignature_) noexcept -> Setup&;
		constexpr auto operator<<(Shader const& shader_) noexcept -> Setup&;
		constexpr auto operator<<(BlendState const& blendState_) noexcept -> Setup&;
		constexpr auto operator<<(RasterizerState const& rasterizerState_) noexcept -> Setup&;
		constexpr auto operator<<(DepthStencilState const& depthStencilState_) noexcept -> Setup&;
		constexpr auto operator<<(InputLayout const& inputLayout_) noexcept -> Setup&;
		constexpr auto operator<<(PrimitiveTopologyType primitiveTopologyType_) noexcept -> Setup&;
		constexpr auto operator<<(std::vector<ResourceFormat> const& rtvFormats_) noexcept -> Setup&;
		constexpr auto operator<<(ResourceFormat dsvFormat_) noexcept -> Setup&;

	private:
		RootSignature const* RootSignature_{ nullptr };
		Shader const* VertexShader_{ nullptr };
		Shader const* PixelShader_{ nullptr };
		BlendState const* BlendState_{ nullptr };
		RasterizerState const* RasterizerState_{ nullptr };
		DepthStencilState const* DepthStencilState_{ nullptr };
		InputLayout const* InputLayout_{ nullptr };
		PrimitiveTopologyType PrimitiveTopologyType_{};
		std::vector<ResourceFormat> const* RTVFormats_{ nullptr };
		ResourceFormat DSVFormat_{};
	};

	constexpr auto GraphicsPipelineState::Setup::operator<<(RootSignature const& rootSignature_)
		noexcept -> Setup& {
		RootSignature_ = &rootSignature_;
		return *this;
	}
	
	constexpr auto GraphicsPipelineState::Setup::operator<<(Shader const& shader_)
		noexcept -> Setup& {
		std::wstring_view profile{ shader_.Profile() };
		if (profile[1] == L's') {
			switch (profile[0]) {
				case L'v': {
					VertexShader_ = &shader_;
					break;
				}
				case L'p': {
					PixelShader_ = &shader_;
					break;
				}
			}
		}
		return *this;
	}

	constexpr auto GraphicsPipelineState::Setup::operator<<(BlendState const& blendState_)
		noexcept -> Setup& {
		BlendState_ = &blendState_;
		return *this;
	}

	constexpr auto GraphicsPipelineState::Setup::operator<<(RasterizerState const& rasterizerState_)
		noexcept -> Setup& {
		RasterizerState_ = &rasterizerState_;
		return *this;
	}

	constexpr auto GraphicsPipelineState::Setup::operator<<(DepthStencilState const& depthStencilState_)
		noexcept -> Setup& {
		DepthStencilState_ = &depthStencilState_;
		return *this;
	}

	constexpr auto GraphicsPipelineState::Setup::operator<<(InputLayout const& inputLayout_)
		noexcept -> Setup& {
		InputLayout_ = &inputLayout_;
		return *this;
	}

	constexpr auto GraphicsPipelineState::Setup::operator<<(PrimitiveTopologyType primitiveTopologyType_)
		noexcept -> Setup& {
		PrimitiveTopologyType_ = primitiveTopologyType_;
		return *this;
	}

	constexpr auto GraphicsPipelineState::Setup::operator<<(std::vector<ResourceFormat> const& rtvFormats_)
		noexcept -> Setup& {
		RTVFormats_ = &rtvFormats_;
		return *this;
	}

	constexpr auto GraphicsPipelineState::Setup::operator<<(ResourceFormat dsvFormat_)
		noexcept -> Setup& {
		DSVFormat_ = dsvFormat_;
		return *this;
	}
}

//////	//////	//////	//////	//////	//////
//	GraphicsPipelineState::InputLayout		//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	class GraphicsPipelineState::InputLayout {
	public:
		auto operator()() const -> std::vector<D3D12_INPUT_ELEMENT_DESC>;

		//----	------	------	------	------	----//

	public:
		void Append(
			std::string_view semanticName_,
			uint32_t semanticIndex_,
			DXGI_FORMAT format_,
			uint32_t inputSlot_ = 0U,
			uint32_t alignedByteOffset_ = D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION inputSlotClass_ = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			uint32_t instanceDataStepRate_ = 0U
		);

		//====	======	======	======	======	====//

	private:
		struct Element {
			std::string SemanticName;
			uint32_t SemanticIndex;
			DXGI_FORMAT Format;
			uint32_t InputSlot;
			uint32_t AlignedByteOffset;
			D3D12_INPUT_CLASSIFICATION InputSlotClass;
			uint32_t InstanceDataStepRate;
		};

		std::vector<Element> Elements_{};
	};

	//////	//////	//////	//////	//////	//////

	auto GraphicsPipelineState::InputLayout::operator()()
		const -> std::vector<D3D12_INPUT_ELEMENT_DESC> {
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs{};
		inputElementDescs.reserve(Elements_.size());

		for (auto const& entry : Elements_) {
			inputElementDescs.emplace_back(
				entry.SemanticName.data(),
				entry.SemanticIndex,
				entry.Format,
				entry.InputSlot,
				entry.AlignedByteOffset,
				entry.InputSlotClass,
				entry.InstanceDataStepRate
			);
		}

		return inputElementDescs;
	}

	//----	------	------	------	------	----//

	void GraphicsPipelineState::InputLayout::Append(
		std::string_view semanticName_,
		uint32_t semanticIndex_,
		DXGI_FORMAT format_,
		uint32_t inputSlot_,
		uint32_t alignedByteOffset_,
		D3D12_INPUT_CLASSIFICATION inputSlotClass_,
		uint32_t instanceDataStepRate_
	) {
		Elements_.emplace_back(
			std::string{ semanticName_ },
			semanticIndex_,
			format_,
			inputSlot_,
			alignedByteOffset_,
			inputSlotClass_,
			instanceDataStepRate_
		);
	}
}

//////	//////	//////	//////	//////	//////
//	GraphicsPipelineState					//
//	ComputePipelineState					//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	GraphicsPipelineState					//
	//////	//////	//////	//////	//////	//////

	void GraphicsPipelineState::Initialize(
		GraphicsDevice const& device_,
		RootSignature const& rs_,
		Shader const& vertexShader_,
		Shader const& pixelShader_,
		BlendState const& blendState_,
		RasterizerState const& rasterizerState_,
		DepthStencilState const& depthStencilState_,
		InputLayout const& inputLayout_,
		PrimitiveTopologyType primitiveTopologyType_,
		std::vector<ResourceFormat> const& rtvFormats_,
		ResourceFormat dsvFormat_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		auto&& inputElements{ inputLayout_() };

		uint32_t num_RenderTargets{ static_cast<uint32_t>(rtvFormats_.size()) };
		(num_RenderTargets > 0U) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.GraphicsPSO - {}> No RTV formats have been specified!\n",
				debugName_
			)
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{
			.pRootSignature{ rs_.Get() },
			.VS{ vertexShader_->GetBufferPointer(), vertexShader_->GetBufferSize() },
			.PS{ pixelShader_->GetBufferPointer(), pixelShader_->GetBufferSize() },
			.BlendState{ blendState_ },
			.SampleMask{ D3D12_DEFAULT_SAMPLE_MASK },
			.RasterizerState{ rasterizerState_ },
			.DepthStencilState{ depthStencilState_ },
			.InputLayout{ inputElements.data(), static_cast<uint32_t>(inputElements.size())},
			.PrimitiveTopologyType{ primitiveTopologyType_ },
			.NumRenderTargets{ num_RenderTargets },
			.DSVFormat{ dsvFormat_ },
			.SampleDesc{ .Count{ 1U }, },
		};
		for (size_t i{ 0LLU }; i < std::min<size_t>(rtvFormats_.size(), 8LLU); ++i) {
			psoDesc.RTVFormats[i] = rtvFormats_.at(i);
		}

		device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&Wrapped_)) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.GraphicsPSO> Failed to create {}!\n",
				debugName_
			)
		};
		Logger().Message<0U>(
			"GraphicsPSO,{},PSO created successfully.\n", debugName_
		);

		SetDebugName(debugName_);
	}

	void GraphicsPipelineState::Initialize(
		GraphicsDevice const& device_,
		GraphicsPipelineState::Setup const& psoSetup_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		auto&& inputElements{ (*psoSetup_.InputLayout_)() };

		uint32_t num_RenderTargets{ static_cast<uint32_t>(psoSetup_.RTVFormats_->size()) };
		(num_RenderTargets > 0U) ||
			Utils::Debug::ThrowIfFalse{
				std::format(
					"<DX12.GraphicsPSO - {}> No RTV formats have been specified!\n",
					debugName_
				)
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{
			.pRootSignature{ psoSetup_.RootSignature_->Get() },
			.VS{ (*psoSetup_.VertexShader_)->GetBufferPointer(), (*psoSetup_.VertexShader_)->GetBufferSize() },
			.PS{ (*psoSetup_.PixelShader_)->GetBufferPointer(), (*psoSetup_.PixelShader_)->GetBufferSize() },
			.BlendState{ *psoSetup_.BlendState_ },
			.SampleMask{ D3D12_DEFAULT_SAMPLE_MASK },
			.RasterizerState{ *psoSetup_.RasterizerState_ },
			.DepthStencilState{ *psoSetup_.DepthStencilState_ },
			.InputLayout{ inputElements.data(), static_cast<uint32_t>(inputElements.size())},
			.PrimitiveTopologyType{ psoSetup_.PrimitiveTopologyType_ },
			.NumRenderTargets{ num_RenderTargets },
			.DSVFormat{ psoSetup_.DSVFormat_ },
			.SampleDesc{ .Count{ 1U }, },
		};
		for (size_t i{ 0LLU }; i < std::min<size_t>(psoSetup_.RTVFormats_->size(), 8LLU); ++i) {
			psoDesc.RTVFormats[i] = psoSetup_.RTVFormats_->at(i);
		}

		device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&Wrapped_)) ||
			Utils::Debug::ThrowIfFailed{
				std::format(
					"<DX12.GraphicsPSO> Failed to create {}!\n",
					debugName_
				)
		};
		Logger().Message<0U>(
			"GraphicsPSO,{},PSO created successfully.\n", debugName_
		);

		SetDebugName(debugName_);
	}

	//////	//////	//////	//////	//////	//////
	//	ComputePipelineState					//
	//////	//////	//////	//////	//////	//////

	void ComputePipelineState::Initialize(
		GraphicsDevice const& device_,
		RootSignature const& rs_,
		Shader const& computeShader_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		D3D12_COMPUTE_PIPELINE_STATE_DESC const psoDesc{
			.pRootSignature{ rs_.Get() },
			.CS{ computeShader_->GetBufferPointer(), computeShader_->GetBufferSize() },
		};
		device_->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&Wrapped_)) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.ComputePSO> Failed to create {}!\n",
				debugName_
			)
		};
		Logger().Message<0U>(
			"ComputePSO,{},PSO created successfully.\n",
			debugName_
		);

		SetDebugName(debugName_);
	}
}