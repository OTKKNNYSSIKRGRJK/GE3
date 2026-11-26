export module Lumina.DX12 : RenderPass;

import <memory>;

import <vector>;

import <d3d12.h>;

import : Command;

import Lumina.Math.Numerics;

namespace Lumina::DX12 {
	export class RenderPass {
		struct DescCollection {
			std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> RenderTargetDescs_;
		};
		struct DescCollectionWithDepthStencil : public DescCollection {
			D3D12_RENDER_PASS_DEPTH_STENCIL_DESC DepthStencilDesc_;
		};

		class RenderTargetSetup;
		class DepthStencilSetup;

		class BeginningAccessSetup;
		class EndingAccessSetup;

	public:
		void Begin(
			CommandList const& cmdList_,
			D3D12_RENDER_PASS_FLAGS renderPassFlags_ = D3D12_RENDER_PASS_FLAG_NONE
		){
			CommandList_ = &cmdList_;

			static_cast<ID3D12GraphicsCommandList4*>(CommandList_->Get())->BeginRenderPass(
				static_cast<uint32_t>(DescCollection_->RenderTargetDescs_.size()),
				DescCollection_->RenderTargetDescs_.data(),
				(Flag_UseDepthStencil_) ?
				(&(static_cast<DescCollectionWithDepthStencil*>(DescCollection_.get())->DepthStencilDesc_)) :
				(nullptr),
				renderPassFlags_
			);
		}

		void End() {
			static_cast<ID3D12GraphicsCommandList4*>(CommandList_->Get())->EndRenderPass();
		}

	public:
		[[nodiscard]] auto RenderTarget(uint32_t idx_) const noexcept -> RenderTargetSetup& {
			return reinterpret_cast<RenderTargetSetup&>(
				DescCollection_->RenderTargetDescs_.at(idx_)
			);
		}
		[[nodiscard]] auto DepthStencil() const noexcept -> DepthStencilSetup& {
			return reinterpret_cast<DepthStencilSetup&>(
				static_cast<DescCollectionWithDepthStencil*>(DescCollection_.get())->DepthStencilDesc_
			);
		}

	public:
		void Initialize(
			uint32_t num_RenderTargets_,
			int32_t flag_UseDepthStencil_
		) {
			Flag_UseDepthStencil_ = !!flag_UseDepthStencil_;
			DescCollection_.reset(
				(Flag_UseDepthStencil_) ?
				(new DescCollectionWithDepthStencil{}) :
				(new DescCollection{})
			);
			DescCollection_->RenderTargetDescs_.resize(num_RenderTargets_);
		}

	public:
		CommandList const* CommandList_{ nullptr };
		
		std::unique_ptr<DescCollection> DescCollection_{ nullptr };
		int32_t Flag_UseDepthStencil_{};
	};

	class RenderPass::RenderTargetSetup final :
		protected D3D12_RENDER_PASS_RENDER_TARGET_DESC {

	public:
		[[nodiscard]] constexpr auto View()
			noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE& { return cpuDescriptor; }

		[[nodiscard]] inline auto BeginningEvent()
			noexcept -> BeginningAccessSetup& { return reinterpret_cast<BeginningAccessSetup&>(BeginningAccess); }
		[[nodiscard]] inline auto EndingEvent()
			noexcept -> EndingAccessSetup& { return reinterpret_cast<EndingAccessSetup&>(EndingAccess); }
	};

	class RenderPass::DepthStencilSetup final :
		protected D3D12_RENDER_PASS_DEPTH_STENCIL_DESC {

	public:
		[[nodiscard]] constexpr auto View()
			noexcept -> D3D12_CPU_DESCRIPTOR_HANDLE& { return cpuDescriptor; }

		[[nodiscard]] inline auto DepthBeginningEvent()
			noexcept -> BeginningAccessSetup& { return reinterpret_cast<BeginningAccessSetup&>(DepthBeginningAccess); }
		[[nodiscard]] inline auto StencilBeginningEvent()
			noexcept -> BeginningAccessSetup& { return reinterpret_cast<BeginningAccessSetup&>(StencilBeginningAccess); }
		
		[[nodiscard]] inline auto DepthEndingEvent()
			noexcept -> EndingAccessSetup& { return reinterpret_cast<EndingAccessSetup&>(DepthEndingAccess); }
		[[nodiscard]] inline auto StencilEndingEvent()
			noexcept -> EndingAccessSetup& { return reinterpret_cast<EndingAccessSetup&>(StencilEndingAccess); }
	};

	class RenderPass::BeginningAccessSetup final :
		protected D3D12_RENDER_PASS_BEGINNING_ACCESS {
	public:
		constexpr void NoAccess() noexcept { Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS; }
		constexpr void Discard() noexcept { Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD; }
		constexpr void Preserve() noexcept { Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE; }

		constexpr void ClearTarget(
			DXGI_FORMAT format_,
			Float4 const& clearColor_
		) noexcept {
			Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			Clear.ClearValue.Format = format_;
			Clear.ClearValue.Color[0] = clearColor_.x;
			Clear.ClearValue.Color[1] = clearColor_.y;
			Clear.ClearValue.Color[2] = clearColor_.z;
			Clear.ClearValue.Color[3] = clearColor_.w;
		}

		constexpr void ClearTarget(
			DXGI_FORMAT format_,
			D3D12_DEPTH_STENCIL_VALUE const& clearDS_
		) noexcept {
			Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
			Clear.ClearValue.Format = format_;
			Clear.ClearValue.DepthStencil = clearDS_;
		}
	};


	class RenderPass::EndingAccessSetup final :
		protected D3D12_RENDER_PASS_ENDING_ACCESS {
	public:
		constexpr void NoAccess() { Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS; }
		constexpr void Discard() noexcept { Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD; }
		constexpr void Preserve() noexcept { Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE; }
	};
}