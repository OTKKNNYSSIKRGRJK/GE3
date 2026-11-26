export module Lumina.DX12 : Fence;

//****	******	******	******	******	****//

import <mutex>;

import <d3d12.h>;

import : GraphicsDevice;

import : Wrapper;

import Lumina.Mixins;

//////	//////	//////	//////	//////	//////

// Reference: https://alextardif.com/D3D11To12P1.html

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	class Fence final :
		public Wrapper<Fence, ID3D12Fence>,
		private NonCopyable<Fence> {

		//====	======	======	======	======	====//

	public:
		// Sends a signal to the GPU that it update the fence value.
		// The GPU will receive the signal when reaching here.
		// Locks the critical section lest the fence value be concurrently altered.
		// Mutex will be unlocked upon calling of the destructor of lockGuard,
		// in this case at the end of the function. 
		uint64_t SignalFrom(ID3D12CommandQueue* cmdQueue_) {
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

		// GetCompletedValue is "not as cheap", hence preferably called only when necessary.
		inline void CheckLastCompletedValue(uint64_t checkValue_) noexcept {
			if (checkValue_ > LastCompletedValue_) {
				LastCompletedValue_ = std::max<uint64_t>(LastCompletedValue_, Wrapped_->GetCompletedValue());
			}
		}

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			std::string_view debugName_
		);

		//----	------	------	------	------	----//

	public:
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