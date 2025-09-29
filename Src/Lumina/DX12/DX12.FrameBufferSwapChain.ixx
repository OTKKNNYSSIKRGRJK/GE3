module;

#include<d3d12.h>
#include<dxgi1_6.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : FrameBufferSwapChain;

//****	******	******	******	******	****//

import <cstdint>;

import <memory>;

import <vector>;

import <string>;
import <format>;

import : GraphicsDevice;
import : Command;
import : Descriptor;
import : Resource;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.WinApp;

import Lumina.Utils.String;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

export namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	FrameBufferSwapChain					//
	//////	//////	//////	//////	//////	//////

	class FrameBufferSwapChain final : 
		public Wrapper<FrameBufferSwapChain, IDXGISwapChain4>,
		public NonCopyable<FrameBufferSwapChain> {
	private:
		class Impl;

		//====	======	======	======	======	====//

	public:
		void BeginFrame(
			const CommandList& cmdList_,
			const D3D12_VIEWPORT& viewport_,
			const D3D12_RECT& scissorRect_,
			const float clearColor_[4]
		);
		void EndFrame(
			CommandQueue& cmdQueue_,
			const CommandAllocator& cmdAllocator_,
			const CommandList& cmdList_
		);
		void Present() const;
		void WaitFlip(uint32_t milliseconds_ = 1000U) const;

		//----	------	------	------	------	----//

	public:
		const DXGI_SWAP_CHAIN_DESC1& Desc() const noexcept;
		const D3D12_RENDER_TARGET_VIEW_DESC& RTVDesc() const noexcept;
		uint32_t Num_Buffers() const noexcept;
		D3D12_CPU_DESCRIPTOR_HANDLE RTVCPUHandle(uint32_t idx_) const noexcept;
		D3D12_CPU_DESCRIPTOR_HANDLE DSVCPUHandle() const noexcept;
		ID3D12Resource* BufferResource(uint32_t idx_) const;

		//----	------	------	------	------	----//

	public:
		void Initialize(
			const CommandQueue& graphicsQueue_,
			const WinApp::Window& windowInstance_,
			uint32_t num_Buffers_ = 2U,
			std::string_view debugName_ = "SwapChain"
		);

		//----	------	------	------	------	----//
		
	public:
		constexpr FrameBufferSwapChain() noexcept;
		virtual ~FrameBufferSwapChain() noexcept;

		//====	======	======	======	======	====//

	private:
		Impl* Impl_{ nullptr };
	};
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	FrameBufferSwapChain::Impl				//
	//////	//////	//////	//////	//////	//////
	
	class FrameBufferSwapChain::Impl {
		friend FrameBufferSwapChain;

		//====	======	======	======	======	====//

	public:
		void BeginFrame(
			const CommandList& cmdList_,
			const D3D12_VIEWPORT& viewport_,
			const D3D12_RECT& scissorRect_,
			const float clearColor_[4]
		) {
			auto backBufferIndex{ Owner_->GetCurrentBackBufferIndex() };

			cmdList_.TransitionResourceState(
				Buffers_.at(backBufferIndex),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);

			auto rtv{ RTVHeap_.CPUHandle(backBufferIndex) };
			auto dsv{ DSVHeap_.CPUHandle(0U) };
			// Sets the current render target(s).
			cmdList_->OMSetRenderTargets(1U, &rtv, false, &dsv);
			cmdList_->ClearRenderTargetView(rtv, clearColor_, 0U, nullptr);
			cmdList_->RSSetViewports(1U, &viewport_);
			cmdList_->RSSetScissorRects(1U, &scissorRect_);
			cmdList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0U, 0U, nullptr);
		}

		void EndFrame(
			CommandQueue& cmdQueue_,
			const CommandAllocator& cmdAllocator_,
			const CommandList& cmdList_
		) {
			auto backBufferIndex{ Owner_->GetCurrentBackBufferIndex() };
			cmdList_.TransitionResourceState(
				Buffers_.at(backBufferIndex),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT
			);

			// Batches the command list(s) into the command queue,
			// and the batched command list(s) will be closed.
			cmdQueue_ << cmdList_;
			// Executes the batched command list(s).
			// The fence value is useful for knowing when the GPU will finish executing the commands.
			uint64_t fenceValue{ cmdQueue_.ExecuteBatchedCommandLists() };
			// Notifies the GPU and the OS to swap the buffers.
			Present();
			// Makes the CPU wait for the GPU.
			cmdQueue_.CPUWait(fenceValue);

			WaitFlip(1000U);

			// Prepares a command list for the next frame.
			cmdList_.Reset(cmdAllocator_);
		}

		void Present() const {
			static constexpr int32_t refreshRateThreshold{ 58 };
			[[maybe_unused]] HRESULT result{ Owner_->Present((RefreshRate_ < refreshRateThreshold) ? (0U) : (1U), 0U) };
		}

		void WaitFlip(uint32_t milliseconds_) const {
			WaitForSingleObject(FrameLatencyWaitableObject_, milliseconds_);
		}

		//----	------	------	------	------	----//

	private:
		void GenerateResourceDesc(const WinApp::Window& windowInstance_) {
			Desc_.Width = windowInstance_.ClientWidth();			// Width of the buffer(s)
			Desc_.Height = windowInstance_.ClientHeight();			// Height of the buffer(s)
			Desc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;				// Color format
			Desc_.SampleDesc.Count = 1U;							// No multisampling
			Desc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// Used as render targets
			Desc_.BufferCount = Num_Buffers_;						// Double buffering
			// Contents of a back buffer data will be discarded upon being presented on the monitor.
			// This flag cannot be used with multisampling and partial presentation.
			// Reference: https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect
			Desc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			Desc_.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		}

		void CreateSwapChain(
			const CommandQueue& cmdQueue_,
			const WinApp::Window& windowInstance_,
			std::string_view debugName_
		) {
			cmdQueue_.Device().Factory()->CreateSwapChainForHwnd(
				cmdQueue_.Get(),
				windowInstance_.Handle(),
				&Desc_,
				nullptr,
				nullptr,
				reinterpret_cast<IDXGISwapChain1**>(Owner_.GetAddressOf())
			) ||
			Utils::Debug::ThrowIfFailed{
				std::format(
					"<DX12.FrameBufferSwapChain> Failed to create{}!\n",
					debugName_
				)
			};
			Logger().Message<0U>(
				"FrameBufferSwapChain,{},Swap chain created successfully.\n",
				debugName_
			);

			Owner_->SetMaximumFrameLatency(1U);
			FrameLatencyWaitableObject_ = Owner_->GetFrameLatencyWaitableObject();
			::WaitForSingleObject(FrameLatencyWaitableObject_, INFINITE);
		}

		void ObtainBuffers(std::string_view debugName_) {
			for (uint32_t idx_Buffer{ 0U }; idx_Buffer < Num_Buffers_; ++idx_Buffer) {
				auto& buffer{ Buffers_.emplace_back() };

				Owner_->GetBuffer(idx_Buffer, IID_PPV_ARGS(&buffer)) ||
				Utils::Debug::ThrowIfFailed{
					std::format(
						"<DX12.FrameBufferSwapChain - {}> Failed to obtain buffer #{} from the swap chain!\n",
						debugName_,
						idx_Buffer
					)
				};
				Logger().Message<0U>(
					"FrameBufferSwapChain,{},Buffer #{} obtained successfully.\n",
					debugName_,
					idx_Buffer
				);

				#if defined(_DEBUG)
				std::string const bufferDebugName{ std::format("{}.Buffer #{}", debugName_, idx_Buffer) };
				buffer->SetPrivateData(
					WKPDID_D3DDebugObjectName,
					static_cast<uint32_t>(bufferDebugName.size()),
					bufferDebugName.data()
				);
				#endif
			}
		}

		void CreateRTVs(const GraphicsDevice& device_) {
			RTVHeap_.Initialize(
				device_,
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				Num_Buffers_,
				false
			);

			RTVDesc_ = D3D12_RENDER_TARGET_VIEW_DESC{
				.Format{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },				// Output to be converted into SRGB
				.ViewDimension{ D3D12_RTV_DIMENSION_TEXTURE2D },		// Outputted as 2D texture
			};
			for (uint32_t i{ 0U }; i < Num_Buffers_; ++i) {
				D3D12_CPU_DESCRIPTOR_HANDLE rtvCPUHandle{ RTVHeap_.Allocate(1U).CPUHandle(0U) };
				device_->CreateRenderTargetView(Buffers_[i], &RTVDesc_, rtvCPUHandle);
			}
		}

		void CreateDSTexturesAndDSVs(const GraphicsDevice& device_) {
			DSTexture_.Initialize(device_, Desc_.Width, Desc_.Height, "SwapChainDSTex");

			DSVHeap_.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1U, false);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
				.Format{ DSTexture_.Format() },
				.ViewDimension{ D3D12_DSV_DIMENSION_TEXTURE2D },
			};
			D3D12_CPU_DESCRIPTOR_HANDLE dsvCPUHandle{ DSVHeap_.Allocate(1U).CPUHandle(0U) };
			device_->CreateDepthStencilView(DSTexture_.Get(), &dsvDesc, dsvCPUHandle);
		}

		//----	------	------	------	------	----//

	public:
		void Initialize(
			const CommandQueue& cmdQueue_,
			const WinApp::Window& windowInstance_,
			uint32_t num_Buffers_,
			std::string_view debugName_
		) {
			Num_Buffers_ = num_Buffers_;

			GenerateResourceDesc(windowInstance_);
			CreateSwapChain(cmdQueue_, windowInstance_, debugName_);
			ObtainBuffers(debugName_);
			CreateRTVs(cmdQueue_.Device());
			CreateDSTexturesAndDSVs(cmdQueue_.Device());

			HDC hdc{ ::GetDC(windowInstance_.Handle()) };
			RefreshRate_ = ::GetDeviceCaps(hdc, VREFRESH);
			::ReleaseDC(windowInstance_.Handle(), hdc);
		}

		//----	------	------	------	------	----//

	public:
		Impl(FrameBufferSwapChain& owner_) : Owner_{ owner_ } {}
		~Impl() noexcept {
			for (auto&& it{ Buffers_.cbegin() }; it != Buffers_.cend(); ++it) {
				auto* buffer{ *it };
				buffer->Release();

				#if defined(_DEBUG)
				char str[64]{ 0 };
				uint32_t size{ 64U };
				buffer->GetPrivateData(WKPDID_D3DDebugObjectName, &size, str);
				Logger().Message<0U>("FrameBufferSwapChain,{},{} Released.\n", Owner_.DebugName(), str);
				#endif
			}

			Buffers_.clear();
		}

		//====	======	======	======	======	====//

	private:
		DXGI_SWAP_CHAIN_DESC1 Desc_{};
		std::vector<ID3D12Resource*> Buffers_{};
		DescriptorHeap RTVHeap_{};
		D3D12_RENDER_TARGET_VIEW_DESC RTVDesc_{};
		DepthStencilTexture2D DSTexture_{};
		DescriptorHeap DSVHeap_{};
		uint32_t Num_Buffers_{ 2U };
		uint32_t Index_BackBuffer_{};
		int RefreshRate_{};
		HANDLE FrameLatencyWaitableObject_{};

		//----	------	------	------	------	----//

	private:
		FrameBufferSwapChain& Owner_;
	};

	//////	//////	//////	//////	//////	//////
	//	FrameBufferSwapChain					//
	//////	//////	//////	//////	//////	//////

	void FrameBufferSwapChain::BeginFrame(
		const CommandList& cmdList_,
		const D3D12_VIEWPORT& viewport_,
		const D3D12_RECT& scissorRect_,
		const float clearColor_[4]
	) {
		Impl_->BeginFrame(
			cmdList_,
			viewport_,
			scissorRect_,
			clearColor_
		);
	}

	void FrameBufferSwapChain::EndFrame(
		CommandQueue& cmdQueue_,
		const CommandAllocator& cmdAllocator_,
		const CommandList& cmdList_
	) {
		Impl_->EndFrame(
			cmdQueue_,
			cmdAllocator_,
			cmdList_
		);
	}

	void FrameBufferSwapChain::Present() const { Impl_->Present(); }

	void FrameBufferSwapChain::WaitFlip(uint32_t milliseconds_) const { Impl_->WaitFlip(milliseconds_); }

	//----	------	------	------	------	----//

	const DXGI_SWAP_CHAIN_DESC1& FrameBufferSwapChain::Desc() const noexcept { return Impl_->Desc_; }
	const D3D12_RENDER_TARGET_VIEW_DESC& FrameBufferSwapChain::RTVDesc() const noexcept { return Impl_->RTVDesc_; }
	uint32_t FrameBufferSwapChain::Num_Buffers() const noexcept { return Impl_->Num_Buffers_; }
	D3D12_CPU_DESCRIPTOR_HANDLE FrameBufferSwapChain::RTVCPUHandle(uint32_t index_) const noexcept { return Impl_->RTVHeap_.CPUHandle(index_); }
	D3D12_CPU_DESCRIPTOR_HANDLE FrameBufferSwapChain::DSVCPUHandle() const noexcept { return Impl_->DSVHeap_.CPUHandle(0U); }
	ID3D12Resource* FrameBufferSwapChain::BufferResource(uint32_t index_) const { return Impl_->Buffers_.at(index_); }

	//----	------	------	------	------	----//

	void FrameBufferSwapChain::Initialize(
		const CommandQueue& graphicsQueue_,
		const WinApp::Window& windowInstance_,
		uint32_t num_Buffers_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		(Impl_ == nullptr) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.FrameBufferSwapChain - {}> Pointer to Implementation is not nullptr!\n",
				debugName_
			)
		};

		Impl_ = new Impl{ *this };
		Impl_->Initialize(graphicsQueue_, windowInstance_, num_Buffers_, debugName_);

		#if defined(_DEBUG)
		SetDebugName(debugName_);
		#endif
	}

	//----	------	------	------	------	----//

	constexpr FrameBufferSwapChain::FrameBufferSwapChain() noexcept {}

	FrameBufferSwapChain::~FrameBufferSwapChain() {
		if (Impl_ != nullptr) {
			delete Impl_;
			Impl_ = nullptr;
		}
	}
}