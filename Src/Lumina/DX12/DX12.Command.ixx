module;

#include<d3d12.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : Command;

//****	******	******	******	******	****//

import <cstdint>;

import <memory>;

import <vector>;

import <string>;
import <format>;

import <mutex>;

import : GraphicsDevice;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	class CommandQueue;
	class CommandAllocator;
	class CommandList;

	class Fence;
}

//****	******	******	******	******	****//

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	CommandQueue							//
	//////	//////	//////	//////	//////	//////
	
	// Responsible for executing the command list(s) on it,
	// after which the commands in the command list(s) are run on the GPU.
	export class CommandQueue final :
		public Wrapper<CommandQueue, ID3D12CommandQueue>,
		public NonCopyable<CommandQueue> {
	public:
		inline CommandQueue& operator<<(CommandList const& cmdList_);

		//----	------	------	------	------	----//

	public:
		// Puts a command list into a batch that can be later on executed by the command queue;
		// the command list will be CLOSED upon being batched.
		void BatchCommandList(CommandList const& cmdList_);
		// Executes the batched command lists, after which signals the GPU to update the fence value;
		// returns the fence value which other processes could wait on when needed.
		uint64_t ExecuteBatchedCommandLists();

		inline DWORD CPUWait(uint64_t fenceValue_);
		inline DWORD CPUWait();
		inline DWORD SignalAndCPUWait();
		inline void GPUWait(CommandQueue const& cmdQueue_, uint64_t fenceValue_);

		//----	------	------	------	------	----//

	public:
		GraphicsDevice const& Device() const noexcept { return *Device_; }
		D3D12_COMMAND_LIST_TYPE Type() const noexcept { return Type_; }

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT,
			std::string_view debugName_ = "CommandQueue"
		);

		//----	------	------	------	------	----//

	public:
		constexpr CommandQueue() noexcept;
		virtual ~CommandQueue() noexcept;

		//====	======	======	======	======	====//

	private:
		GraphicsDevice const* Device_{ nullptr };
		D3D12_COMMAND_LIST_TYPE Type_{};

		std::vector<ID3D12CommandList*> BatchedCommandLists_{};

		Fence* Fence_{ nullptr };
	};

	//////	//////	//////	//////	//////	//////
	//	CommandAllocator						//
	//////	//////	//////	//////	//////	//////
	
	// Backing memory for recording the GPU commands into a command list.
	export class CommandAllocator final :
		public Wrapper<CommandAllocator, ID3D12CommandAllocator>,
		public NonCopyable<CommandAllocator> {
	public:
		D3D12_COMMAND_LIST_TYPE Type() const noexcept { return Type_; }

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT,
			std::string_view debugName_ = "CommandAllocator"
		);

		//----	------	------	------	------	----//

	public:
		constexpr CommandAllocator() noexcept = default;
		constexpr virtual ~CommandAllocator() noexcept = default;

		//====	======	======	======	======	====//

	private:
		D3D12_COMMAND_LIST_TYPE Type_{};
	};

	//////	//////	//////	//////	//////	//////
	//	CommandList								//
	//////	//////	//////	//////	//////	//////
	
	// Used to issue copy, compute (dispatch), or draw commands.
	// In DirectX 12, the commands in a command list are only run on the GPU
	// after they have been executed on a command queue.
	export class CommandList :
		public Wrapper<CommandList, ID3D12GraphicsCommandList>,
		public NonCopyable<CommandList> {
	public:
		void TransitionResourceState(
			ID3D12Resource* resource_,
			D3D12_RESOURCE_STATES currentState_,
			D3D12_RESOURCE_STATES nextState_
		) const;

		void Reset(CommandAllocator const& cmdAllocator_) const;

		//----	------	------	------	------	----//

	public:
		D3D12_COMMAND_LIST_TYPE Type() const noexcept { return Type_; }

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			CommandAllocator const& cmdAllocator_,
			std::string_view debugName_ = "CommandList"
		);

		//----	------	------	------	------	----//

	public:
		constexpr CommandList() noexcept = default;
		constexpr virtual ~CommandList() noexcept = default;

		//====	======	======	======	======	====//

	private:
		D3D12_COMMAND_LIST_TYPE Type_{};
	};
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	Fence									//
//////	//////	//////	//////	//////	//////

// Reference: https://alextardif.com/D3D11To12P1.html

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	class Fence final :
		public Wrapper<Fence, ID3D12Fence>,
		private NonCopyable<Fence> {
		friend CommandQueue;

		//====	======	======	======	======	====//

	private:
		// Sends a signal to the GPU that it update the fence value.
		// The GPU will receive the signal when reaching here.
		// Locks the critical section lest the fence value be concurrently altered.
		// Mutex will be unlocked upon calling of the destructor of lockGuard,
		// in this case at the end of the function. 
		uint64_t SignalFrom(CommandQueue const& cmdQueue_) {
			std::lock_guard<std::mutex> lockGuard{ Mutex_Signal_ };

			++NextValue_;
			cmdQueue_->Signal(Wrapped_, NextValue_);
			return NextValue_;
		}

		inline DWORD CPUWait() {
			return CPUWait(NextValue_);
		}

		DWORD CPUWait(uint64_t value_) {
			std::lock_guard<std::mutex> lockGuard{ Mutex_CPUWait_ };

			DWORD result_Wait{ WAIT_OBJECT_0 };
			CheckLastCompletedValue(value_);
			// Checks if the fence value has been updated.
			if (LastCompletedValue_ < value_) {
				// Sets an event which is triggered when the fence value is updated,
				// i.e. the GPU receives the signal.
				Wrapped_->SetEventOnCompletion(value_, Event_);
				result_Wait = ::WaitForSingleObject(Event_, INFINITE);
			}
			return result_Wait;
		}

		// GetCompletedValue is "not as cheap", hence preferably being called only when necessary.
		inline void CheckLastCompletedValue(uint64_t checkValue_) noexcept {
			if (checkValue_ > LastCompletedValue_) {
				LastCompletedValue_ = std::max<uint64_t>(LastCompletedValue_, Wrapped_->GetCompletedValue());
			}
		}

		//----	------	------	------	------	----//

	private:
		void Initialize(
			GraphicsDevice const& device_,
			std::string_view debugName_
		);

		//----	------	------	------	------	----//

	private:
		constexpr Fence() noexcept;
		virtual ~Fence() noexcept;

		//====	======	======	======	======	====//

	private:
		uint64_t NextValue_{ 0ULL };
		uint64_t LastCompletedValue_{ 0ULL };
		HANDLE Event_{ nullptr };

		std::mutex Mutex_Signal_{};
		std::mutex Mutex_CPUWait_{};
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	void Fence::Initialize(
		GraphicsDevice const& device_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		device_->CreateFence(
			LastCompletedValue_,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.Fence> Failed to create {}!\n",
				debugName_
			)
		};
		Logger().Message<0U>(
			"Fence,{},Fence created successfully.\n",
			debugName_
		);

		Event_ = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		(Event_ != nullptr) ||
		Utils::Debug::ThrowIfFalse{
			"<DX12.Fence> Failed to create a fence event!\n"
		};
		Logger().Message<0U>(
			"Fence,{},Fence event created successfully.\n",
			debugName_
		);

		SetDebugName(debugName_);
	}

	//----	------	------	------	------	----//

	constexpr Fence::Fence() noexcept {}

	Fence::~Fence() noexcept {
		if (Event_ != nullptr) {
			::CloseHandle(Event_);
			Event_ = nullptr;
		}
	}
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	CommandQueue							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	inline CommandQueue& CommandQueue::operator<<(CommandList const& cmdList_) {
		BatchCommandList(cmdList_);
		return (*this);
	}

	//----	------	------	------	------	----//

	void CommandQueue::BatchCommandList(CommandList const& cmdList_) {
		// Closes the command list so that the commands inside are "confirmed". 
		[[maybe_unused]] HRESULT hr_CloseCmdList{ cmdList_->Close() };
		#if defined (_DEBUG)
		hr_CloseCmdList ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandQueue - {}> Failed to close {}!\n",
				DebugName(),
				cmdList_.DebugName()
			)
		};
		#endif

		BatchedCommandLists_.emplace_back(cmdList_.Get());
	}

	uint64_t CommandQueue::ExecuteBatchedCommandLists() {
		Wrapped_->ExecuteCommandLists(
			static_cast<uint32_t>(BatchedCommandLists_.size()),
			BatchedCommandLists_.data()
		);

		BatchedCommandLists_.clear();

		return Fence_->SignalFrom(*this);
	}

	//----	------	------	------	------	----//

	inline DWORD CommandQueue::CPUWait(uint64_t fenceValue_) {
		return Fence_->CPUWait(fenceValue_);
	}

	inline DWORD CommandQueue::CPUWait() {
		return Fence_->CPUWait();
	}
	
	inline DWORD CommandQueue::SignalAndCPUWait() {
		Fence_->SignalFrom(*this);
		return Fence_->CPUWait();
	}

	inline void CommandQueue::GPUWait(CommandQueue const& cmdQueue_, uint64_t fenceValue_) {
		Wrapped_->Wait(cmdQueue_.Fence_->Get(), fenceValue_);
	}

	//----	------	------	------	------	----//

	void CommandQueue::Initialize(
		GraphicsDevice const& device_,
		D3D12_COMMAND_LIST_TYPE type_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		D3D12_COMMAND_QUEUE_DESC desc_CmdQueue{
			.Type{ type_ }
		};
		device_->CreateCommandQueue(
			&desc_CmdQueue,
			IID_PPV_ARGS(GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandQueue> Failed to create {}!\n",
				debugName_
			)
		};
		Logger().Message<0U>(
			"CommandQueue,{},Command queue created successfully.\n",
			debugName_
		);

		Device_ = &device_;
		Type_ = type_;

		(Fence_ == nullptr) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandQueue - {}> Pointer to the Fence object must be nullptr!\n",
				debugName_
			)
		};
		Fence_ = new Fence{};
		Fence_->Initialize(
			*Device_,
			std::format("{}.Fence", debugName_)
		);

		SetDebugName(debugName_);
	}

	//----	------	------	------	------	----//

	constexpr CommandQueue::CommandQueue() noexcept {}

	CommandQueue::~CommandQueue() noexcept {
		if (Fence_ != nullptr) {
			delete Fence_;
			Fence_ = nullptr;
		}
	}
}

//////	//////	//////	//////	//////	//////
//	CommandAllocator						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	void CommandAllocator::Initialize(
		GraphicsDevice const& device_,
		D3D12_COMMAND_LIST_TYPE type_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		device_->CreateCommandAllocator(
			type_,
			IID_PPV_ARGS(GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandAllocator> Failed to create {}!\n",
				debugName_
			)
		};
		Logger().Message<0U>(
			"CommandAllocator,{},Command allocator created successfully.\n",
			debugName_
		);

		Type_ = type_;

		SetDebugName(debugName_);
	}
}

//////	//////	//////	//////	//////	//////
//	CommandList								//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	void CommandList::TransitionResourceState(
		ID3D12Resource* resource_,
		D3D12_RESOURCE_STATES currentState_,
		D3D12_RESOURCE_STATES nextState_
	) const {
		D3D12_RESOURCE_BARRIER barrier{
			.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
			.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
			.Transition{
				// Target of the barrier
				.pResource{ resource_ },
				// Resource state before transition
				.StateBefore{ currentState_ },
				// Resource state after transition
				.StateAfter{ nextState_ },
			},
		};

		Wrapped_->ResourceBarrier(1, &barrier);
	}

	void CommandList::Reset(CommandAllocator const& cmdAllocator_) const {
		#if defined(_DEBUG)
		cmdAllocator_->Reset() ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandList - {}> Failed to reset the command allocator ({})!\n",
				DebugName(),
				cmdAllocator_.DebugName()
			)
		};

		Wrapped_->Reset(cmdAllocator_.Get(), nullptr) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandList - {}> Failed to reset the command list!\n",
				DebugName(),
				cmdAllocator_.DebugName()
			)
		};
		#else
		cmdAllocator_->Reset();
		Wrapped_->Reset(cmdAllocator_.Get(), nullptr);
		#endif
	}

	//----	------	------	------	------	----//

	void CommandList::Initialize(
		GraphicsDevice const& device_,
		CommandAllocator const& cmdAllocator_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		device_->CreateCommandList(
			0U,
			cmdAllocator_.Type(),
			cmdAllocator_.Get(),
			nullptr,
			IID_PPV_ARGS(GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandList> Failed to create {}!\n",
				debugName_
			)
		};
		Logger().Message<0U>(
			"CommandList,{},Command list created successfully.\n",
			debugName_
		);

		SetDebugName(debugName_);

		Type_ = cmdAllocator_.Type();

		Wrapped_->Close() ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommandList - {}> Failed to close the command list!\n",
				debugName_
			)
		};
		Reset(cmdAllocator_);
	}
}