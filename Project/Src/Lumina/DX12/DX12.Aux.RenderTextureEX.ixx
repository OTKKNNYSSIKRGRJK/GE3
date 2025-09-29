module;

#include<d3d12.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12.Aux.RenderTextureEX;

//****	******	******	******	******	****//

import <cstdint>;

import <memory>;

import <string>;
import <format>;

import Lumina.DX12;
import Lumina.DX12.Aux.View;

import Lumina.Mixins;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	export class RenderTextureEX : protected RenderTexture2D {
	public:
		void Begin(
			CommandList const& cmdList_,
			D3D12_VIEWPORT const& viewport_,
			D3D12_RECT const& scissorRect_
		);
		void End(
			CommandList const& cmdList_
		);

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint32_t width_,
			uint32_t height_,
			float const clearColor_[4],
			std::string_view debugName_ = "RenderTex"
		);

	public:
		RenderTextureEX() = default;
		virtual ~RenderTextureEX() = default;

	private:
		//// Command allocator for the pre-render command list
		//CommandAllocator CommandAllocator_Begin_{};
		//// Command list in which pre-render commands are to be recorded
		//CommandList CommandList_Begin_{};
		//// Command allocator for the post-render command list
		//CommandAllocator CommandAllocator_End_{};
		//// Command list in which post-render commands are to be recorded
		//CommandList CommandList_End_{};
		
		DescriptorHeap RTVHeap_{};
		DescriptorHeap DSVHeap_{};

		float ClearColor_[4]{};

		DepthStencilTexture2D DSTexture_{};
	};

	void RenderTextureEX::Initialize(
		GraphicsDevice const& device_,
		uint32_t width_,
		uint32_t height_,
		float const clearColor_[4],
		std::string_view debugName_
	) {
		RenderTexture2D::Initialize(
			device_,
			width_,
			height_,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			{ clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3] },
			debugName_
		);
		std::memcpy(ClearColor_, clearColor_, sizeof(float) * 4LLU);

		/*CommandAllocator_Begin_.Initialize(
			device_, D3D12_COMMAND_LIST_TYPE_DIRECT,
			std::format("CmdAllocator_Begin@{}", debugName_)
		);
		CommandList_Begin_.Initialize(
			device_, CommandAllocator_Begin_,
			std::format("CmdList_Begin@{}", debugName_)
		);
		CommandAllocator_End_.Initialize(
			device_, D3D12_COMMAND_LIST_TYPE_DIRECT,
			std::format("CmdAllocator_Begin@{}", debugName_)
		);
		CommandList_End_.Initialize(
			device_, CommandAllocator_End_,
			std::format("CmdList_Begin@{}", debugName_)
		);*/

		DSTexture_.Initialize(device_, width_, height_, debugName_);

		RTVHeap_.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1U, false);
		DSVHeap_.Initialize(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1U, false);
		
		D3D12_RESOURCE_DESC resDesc{
			.Dimension{ D3D12_RESOURCE_DIMENSION_TEXTURE2D },
			.Width{ static_cast<uint64_t>(width_) },
			.Height{ height_ },
			.DepthOrArraySize{ 1U },
			.MipLevels{ 1U },
			.Format{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
			.SampleDesc{ .Count = 1, .Quality = 0 }
		};
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
			.Format{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },				// Output to be converted into SRGB
			.ViewDimension{ D3D12_RTV_DIMENSION_TEXTURE2D },		// Outputted as 2D texture
		};
		device_->CreateRenderTargetView(Wrapped_, &rtvDesc, RTVHeap_.CPUHandle(0U));

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
			.Format{ DSTexture_.Format() },
			.ViewDimension{ D3D12_DSV_DIMENSION_TEXTURE2D },
		};
		device_->CreateDepthStencilView(DSTexture_.Get(), &dsvDesc, DSVHeap_.CPUHandle(0U));
	}

	void RenderTextureEX::Begin(
		CommandList const& cmdList_,
		D3D12_VIEWPORT const& viewport_,
		D3D12_RECT const& scissorRect_
	) {

		cmdList_.TransitionResourceState(
			Wrapped_,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		auto rtv{ RTVHeap_.CPUHandle(0U) };
		auto dsv{ DSVHeap_.CPUHandle(0U) };
		// Set the current render targets.
		cmdList_->OMSetRenderTargets(1, &rtv, false, &dsv);
		cmdList_->ClearRenderTargetView(rtv, ClearColor_, 0, nullptr);
		cmdList_->RSSetViewports(1, &viewport_);
		cmdList_->RSSetScissorRects(1, &scissorRect_);
		cmdList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	}

	void RenderTextureEX::End(
		CommandList const& cmdList_
	) {
		cmdList_.TransitionResourceState(
			Wrapped_,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
	}
}