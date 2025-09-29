export module Lumina.DX12.Context;

import <string>;

import Lumina.DX12;

import Lumina.WinApp.Context;

import Lumina.Mixins;

namespace Lumina::DX12 {
	
	// Revision is certainly needed, but that's it for now.

	export class Context final : public NonCopyable<Context> {
	public:
		auto Device() const noexcept -> GraphicsDevice const& { return Device_; }
		
		auto DirectQueue() const noexcept -> CommandQueue& { return CQ_Direct_; }
		auto CopyQueue() const noexcept -> CommandQueue& { return CQ_Copy_; }
		auto ComputeQueue() const noexcept -> CommandQueue& { return CQ_Compute_; }

		auto GlobalDescriptorHeap() const noexcept -> DescriptorHeap const& { return DH_Global_; }

		auto SwapChain() const noexcept -> FrameBufferSwapChain const& { return SwapChain_; }

	public:
		void Compile(
			Shader& shader_,
			std::wstring_view filePath_,
			std::wstring_view profile_,
			std::wstring_view entryPoint_,
			std::string_view debugName_
		) const;

	public:
		void BeginFrame(CommandList const& cmdList_);
		void EndFrame(CommandAllocator const& cmdAllocator_, CommandList const& cmdList_);

	public:
		void Initialize(WinApp::Context const& winAppContext_);
		void Finalize();

	public:
		constexpr Context() noexcept = default;
		constexpr ~Context() noexcept = default;

	private:
		GraphicsDevice Device_{};

		CommandQueue mutable CQ_Direct_{};
		CommandQueue mutable CQ_Copy_{};
		CommandQueue mutable CQ_Compute_{};

		// Large shader visible descriptor heap
		// in order to minimize the times of calling the costly ID3D12GraphicsCommandList::SetDescriptorHeaps
		DescriptorHeap DH_Global_{};

		FrameBufferSwapChain SwapChain_{};

		Shader::Compiler ShaderCompiler_{};

		D3D12_VIEWPORT Viewport_{};
		D3D12_RECT ScissorRect_{};
		float ClearColor_[4]{ 0.0f, 0.0f, 0.0f, 1.0f };

	private:
		static constinit inline LeakChecker const LeakChecker_{};

	private:
		static constexpr uint32_t Capacity_DH_Global_{ 1024U };
	};

	void Context::Compile(
		Shader& shader_,
		std::wstring_view filePath_,
		std::wstring_view profile_,
		std::wstring_view entryPoint_,
		std::string_view debugName_
	) const {
		shader_.Initialize(
			ShaderCompiler_,
			filePath_,
			profile_,
			entryPoint_,
			debugName_
		);
	}

	void Context::BeginFrame(CommandList const& cmdList_) {
		SwapChain_.BeginFrame(cmdList_, Viewport_, ScissorRect_, ClearColor_);
	}

	void Context::EndFrame(CommandAllocator const& cmdAllocator_, CommandList const& cmdList_) {
		SwapChain_.EndFrame(CQ_Direct_, cmdAllocator_, cmdList_);
	}

	void Context::Initialize(WinApp::Context const& winAppContext_) {
		#if defined(_DEBUG)
		// If the debug layer is to be enabled,
		// the enablement should be done before initializing the D3D12 context.
		EnableDebugLayer();
		#endif

		Device_.Initialize("Device");

		CQ_Direct_.Initialize(Device_, D3D12_COMMAND_LIST_TYPE_DIRECT, "Direct Queue");
		CQ_Copy_.Initialize(Device_, D3D12_COMMAND_LIST_TYPE_COPY, "Copy Queue");
		CQ_Compute_.Initialize(Device_, D3D12_COMMAND_LIST_TYPE_COMPUTE, "Compute Queue");

		DH_Global_.Initialize(
			Device_,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			Capacity_DH_Global_,
			true,
			"CBV/SRV/UAV Heap"
		);

		ShaderCompiler_.Initialize("Shader Compiler");

		auto const& mainWindow{ winAppContext_.WindowInstance(L"Main") };

		SwapChain_.Initialize(CQ_Direct_, mainWindow, 2U, "Swap Chain");

		Viewport_ = {
			.TopLeftX{ 0.0f },
			.TopLeftY{ 0.0f },
			.Width{ static_cast<float>(mainWindow.ClientWidth()) },
			.Height{ static_cast<float>(mainWindow.ClientHeight()) },
			.MinDepth{ 0.0f },
			.MaxDepth{ 1.0f },
		};
		ScissorRect_ = {
			.left{ 0 },
			.top{ 0 },
			.right{ static_cast<int32_t>(mainWindow.ClientWidth()) },
			.bottom{ static_cast<int32_t>(mainWindow.ClientHeight()) },
		};
	}

	void Context::Finalize() {}
}