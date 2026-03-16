export module Lumina.DX12 : Canvas;

import <cstdint>;

import <memory>;

import <vector>;

import <string>;

import <d3d12.h>;

import : GraphicsDevice;
import : Command;
import : Barrier;

import : Resource.Commons;
import : Resource.Texture2D;
import : Descriptor;

namespace Lumina::DX12 {
	export class Canvas {
	public:
		[[nodiscard]] inline auto RenderTexture(uint32_t idx_) noexcept -> RenderTexture2D&;
		[[nodiscard]] inline auto RenderTexture(uint32_t idx_) const noexcept -> RenderTexture2D const&;

		[[nodiscard]] inline auto DepthTexture() noexcept -> DepthTexture2D&;
		[[nodiscard]] inline auto DepthTexture() const noexcept -> DepthTexture2D const&;

		[[nodiscard]] constexpr auto Viewport(uint32_t idx_) noexcept -> D3D12_VIEWPORT&;
		[[nodiscard]] constexpr auto Viewport(uint32_t idx_) const noexcept -> D3D12_VIEWPORT const&;
		[[nodiscard]] constexpr auto Viewports() const noexcept -> std::vector<D3D12_VIEWPORT> const&;

		[[nodiscard]] constexpr auto ScissorRect(uint32_t idx_) noexcept -> D3D12_RECT&;
		[[nodiscard]] constexpr auto ScissorRect(uint32_t idx_) const noexcept -> D3D12_RECT const&;
		[[nodiscard]] constexpr auto ScissorRects() const noexcept -> std::vector<D3D12_RECT> const&;

		[[nodiscard]] constexpr auto RTV(uint32_t idx_) const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE;
		[[nodiscard]] constexpr auto DSV() const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE;

		constexpr auto Num_RenderTargets() const noexcept -> uint32_t;
		constexpr auto Flag_UseDepthTest() const noexcept -> bool;

	public:
		void AllocateTextures(
			uint32_t num_RenderTargets_,
			bool flag_UseDepthTest_
		);
		void TransitionResourceStates(
			GraphicsDevice const& device_,
			CommandQueue& cmdQueue_
		);
		void CreateViews(
			GraphicsDevice const& device_
		);

	public:
		~Canvas();

	private:
		std::vector<std::unique_ptr<Texture2D>> Textures_{};

		std::vector<D3D12_VIEWPORT> Viewports_{};
		std::vector<D3D12_RECT> ScissorRects_{};

		DescriptorHeap RTVHeap_{};
		DescriptorHeap DSVHeap_{};

		uint32_t Num_RenderTargets_{};
		int32_t Flag_UseDepthTest_{};
	};

	inline auto Canvas::RenderTexture(uint32_t idx_)
		noexcept -> RenderTexture2D& {
		return static_cast<RenderTexture2D&>(*(Textures_[idx_]));
	}
	inline auto Canvas::RenderTexture(uint32_t idx_)
		const noexcept -> RenderTexture2D const& {
		return static_cast<RenderTexture2D const&>(*(Textures_[idx_]));
	}

	inline auto Canvas::DepthTexture()
		noexcept -> DepthTexture2D& {
		return static_cast<DepthTexture2D&>(*(Textures_.back()));
	}
	inline auto Canvas::DepthTexture()
		const noexcept -> DepthTexture2D const& {
		return static_cast<DepthTexture2D const&>(*(Textures_.back()));
	}

	constexpr auto Canvas::Viewport(uint32_t idx_)
		noexcept -> D3D12_VIEWPORT& {
		return Viewports_[idx_];
	}
	constexpr auto Canvas::Viewport(uint32_t idx_)
		const noexcept -> D3D12_VIEWPORT const& {
		return Viewports_[idx_];
	}
	constexpr auto Canvas::Viewports()
		const noexcept -> std::vector<D3D12_VIEWPORT> const& {
		return Viewports_;
	}

	constexpr auto Canvas::ScissorRect(uint32_t idx_)
		noexcept -> D3D12_RECT& {
		return ScissorRects_[idx_];
	}
	constexpr auto Canvas::ScissorRect(uint32_t idx_)
		const noexcept -> D3D12_RECT const& {
		return ScissorRects_[idx_];
	}
	constexpr auto Canvas::ScissorRects()
		const noexcept -> std::vector<D3D12_RECT> const& {
		return ScissorRects_;
	}

	constexpr auto Canvas::RTV(uint32_t idx_)
		const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE {
		return RTVHeap_.CPUHandle(idx_);
	}
	constexpr auto Canvas::DSV()
		const noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE {
		return DSVHeap_.CPUHandle(0U);
	}

	constexpr auto Canvas::Num_RenderTargets() const noexcept -> uint32_t { return Num_RenderTargets_; }
	constexpr auto Canvas::Flag_UseDepthTest() const noexcept -> bool { return Flag_UseDepthTest_; }

	void Canvas::AllocateTextures(
		uint32_t num_RenderTargets_,
		bool flag_UseDepthTest_
	) {
		Num_RenderTargets_ = (num_RenderTargets_ < 1U) ? (1U) : (num_RenderTargets_);
		Num_RenderTargets_ = (Num_RenderTargets_ < 8U) ? (Num_RenderTargets_) : (8U);

		Textures_.resize(Num_RenderTargets_);
		for (auto& renderTex : Textures_) {
			renderTex = std::make_unique<RenderTexture2D>();
		}

		Flag_UseDepthTest_ = !!flag_UseDepthTest_;
		if (Flag_UseDepthTest_) {
			auto& depthTex{ Textures_.emplace_back() };
			depthTex = std::make_unique<DepthTexture2D>();
		}

		Viewports_.resize(Num_RenderTargets_);
		ScissorRects_.resize(Num_RenderTargets_);
	}

	void Canvas::TransitionResourceStates(
		GraphicsDevice const& device_,
		CommandQueue& cmdQueue_
	) {
		std::vector<D3D12_RESOURCE_BARRIER> barriers{};
		for (uint32_t idx{ 0U }; idx < Num_RenderTargets_; ++idx) {
			barriers.emplace_back(
				Lumina::DX12::Barrier::Transition(
					RenderTexture(idx),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				)
			);
		}
		if (Flag_UseDepthTest_) {
			barriers.emplace_back(
				Lumina::DX12::Barrier::Transition(
					DepthTexture(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
				)
			);
		}

		CommandAllocator cmdAllocator{};
		cmdAllocator.Initialize(device_);
		CommandList cmdList{};
		cmdList.Initialize(device_, cmdAllocator);

		cmdList->ResourceBarrier(
			static_cast<uint32_t>(barriers.size()),
			barriers.data()
		);
		cmdQueue_ << cmdList;
		cmdQueue_.CPUWait(cmdQueue_.ExecuteBatchedCommandLists());
	}

	void Canvas::CreateViews(
		GraphicsDevice const& device_
	) {
		RTVHeap_.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, Num_RenderTargets_, false);
		for (uint32_t idx{ 0U }; idx < Num_RenderTargets_; ++idx) {
			auto const& renderTex{ RenderTexture(idx) };
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
				.Format{ renderTex.Format() },
				.ViewDimension{ D3D12_RTV_DIMENSION_TEXTURE2D },
				.Texture2D{
					.MipSlice{ 0U },
					.PlaneSlice{ 0U },
				},
			};
			device_->CreateRenderTargetView(
				renderTex.Get(),
				&rtvDesc,
				RTVHeap_.CPUHandle(idx)
			);
		}

		if (Flag_UseDepthTest_) {
			DSVHeap_.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1U, false);
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
				.Format{ DXGI_FORMAT_D24_UNORM_S8_UINT },
				.ViewDimension{ D3D12_DSV_DIMENSION_TEXTURE2D },
			};
			device_->CreateDepthStencilView(
				DepthTexture().Get(),
				&dsvDesc,
				DSVHeap_.CPUHandle(0U)
			);
		}
	}

	Canvas::~Canvas() {
		for (uint32_t idx{ 0U }; idx < Num_RenderTargets_; ++idx) {
			Textures_[idx].reset(nullptr);
		}
		if (Flag_UseDepthTest_) {
			Textures_.back().reset(nullptr);
		}
	}
}