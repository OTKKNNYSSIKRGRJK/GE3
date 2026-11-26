module;

#include<d3d12.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : Descriptor;

//****	******	******	******	******	****//

import <cstdint>;
import <type_traits>;

import <memory>;

import <string>;
import <format>;

import : GraphicsDevice;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	class DescriptorHeap;
	class DescriptorTable;

	namespace {
		template<typename T>
		concept UnsignedIntegral =
			(
				(std::is_integral_v<T> && std::is_unsigned_v<T>) ||
				(std::is_enum_v<T> && std::is_unsigned_v<std::underlying_type_t<T>>)
			);
	}
}

//****	******	******	******	******	****//

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	DescriptorHeap							//
	//////	//////	//////	//////	//////	//////

	// "Collection of contiguous allocations of descriptors, one allocation for every descriptor"
	// Reference: https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps
	export class DescriptorHeap final :
		public Wrapper<DescriptorHeap, ID3D12DescriptorHeap>,
		public NonCopyable<DescriptorHeap> {
	public:
		DescriptorTable Allocate(uint32_t num_Descriptors_) const;
		void Allocate(
			DescriptorTable& descriptorTable_,
			uint32_t num_Descriptors_
		) const;

		//----	------	------	------	------	----//

	public:
		constexpr D3D12_DESCRIPTOR_HEAP_TYPE Type() const noexcept { return Type_; }
		constexpr uint32_t Increment() const noexcept { return Increment_; }

		constexpr uint32_t Capacity() const noexcept { return Capacity_; }
		constexpr uint32_t Size() const noexcept { return CurrentSize_; }

		constexpr bool IsFull() const noexcept { return (CurrentSize_ >= Capacity_); }
		constexpr bool IsVisibleToShader() const noexcept {
			return !!(HeapFlag_ & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
		}

		constexpr D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(uint32_t index_) const noexcept {
			return D3D12_CPU_DESCRIPTOR_HANDLE{
				CPUHandle_Front_.ptr +
				Increment_ * static_cast<size_t>(index_)
			};
		}
		constexpr D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(uint32_t index_) const noexcept {
			return D3D12_GPU_DESCRIPTOR_HANDLE{
				GPUHandle_Front_.ptr +
				Increment_ * static_cast<size_t>(index_)
			};
		}

		//----	------	------	------	------	----//

	public:
		void Initialize(
			const GraphicsDevice& device_,
			D3D12_DESCRIPTOR_HEAP_TYPE type_,
			uint32_t num_Descriptors_,
			bool isVisibleToShader_,
			std::string_view debugName_ = "DescriptorHeap"
		);

		//----	------	------	------	------	----//

	public:
		constexpr DescriptorHeap() noexcept = default;
		constexpr virtual ~DescriptorHeap() noexcept = default;

		//====	======	======	======	======	====//

	private:
		D3D12_DESCRIPTOR_HEAP_TYPE Type_{};
		D3D12_DESCRIPTOR_HEAP_FLAGS HeapFlag_{};
		D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle_Front_{};
		D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle_Front_{};
		uint32_t Increment_{ 0U };

		uint32_t Capacity_{ 0U };
		mutable uint32_t CurrentSize_{ 0U };
	};

	//////	//////	//////	//////	//////	//////
	//	DescriptorTable							//
	//////	//////	//////	//////	//////	//////

	// "Sub-range of a descriptor heap"
	// Reference: https://learn.microsoft.com/en-us/windows/win32/direct3d12/descriptor-tables
	export class DescriptorTable final {
		friend DescriptorHeap;

		//====	======	======	======	======	====//

	public:
		template<UnsignedIntegral IndexType>
		constexpr D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(IndexType idx_) const noexcept {
			//assert(static_cast<uint32_t>(idx_) < Size_);
			return Heap_->CPUHandle(Index_Front_ + static_cast<uint32_t>(idx_));
		}

		template<UnsignedIntegral IndexType>
		constexpr D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(IndexType idx_) const noexcept {
			//assert(static_cast<uint32_t>(idx_) < Size_ && Heap_->IsVisibleToShader());
			return Heap_->GPUHandle(Index_Front_ + static_cast<uint32_t>(idx_));
		}

		constexpr const DescriptorHeap& Heap() const noexcept { return *Heap_; }
		constexpr uint32_t Size() const noexcept { return Size_; }

		//----	------	------	------	------	----//

	public:
		DescriptorTable() noexcept = default;
		~DescriptorTable() noexcept = default;

		//====	======	======	======	======	====//

	private:
		const DescriptorHeap* Heap_{ nullptr };
		uint32_t Size_{ 0U };
		uint32_t Index_Front_{ 0U };
	};
}

//****	******	******	******	******	****//

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	DescriptorHeap							//
	//////	//////	//////	//////	//////	//////

	DescriptorTable DescriptorHeap::Allocate(uint32_t num_Descriptors_) const {
		DescriptorTable descriptorTable{};
		Allocate(descriptorTable, num_Descriptors_);
		return descriptorTable;
	}

	void DescriptorHeap::Allocate(
		DescriptorTable& descriptorTable_,
		uint32_t num_Descriptors_
	) const {
		(num_Descriptors_ > 0U) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"{} : the number of descriptor(s) to allocate must be larger than zero!\n",
				DebugName()
			)
		};
		(CurrentSize_ + num_Descriptors_ <= Capacity_) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"Attempted to allocate new descriptor(s) from {} despite of insufficient capacity!\n",
				DebugName()
			)
		};

		descriptorTable_.Heap_ = this;
		descriptorTable_.Size_ = num_Descriptors_;
		descriptorTable_.Index_Front_ = CurrentSize_;

		CurrentSize_ += num_Descriptors_;
	}

	//----	------	------	------	------	----//

	void DescriptorHeap::Initialize(
		const GraphicsDevice& device_,
		D3D12_DESCRIPTOR_HEAP_TYPE type_,
		uint32_t num_Descriptors_,
		bool isVisibleToShader_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		Type_ = type_;
		HeapFlag_ =
			(isVisibleToShader_) ?
			(D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) :
			(D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		// The size of a descriptor would vary according to different GPUs and drivers in order for optimization.
		// GetDescriptorHandleIncrementSize is therefore called upon initialization
		// to get the actual descriptor size, which should be constant during the execution of the application.
		Increment_ = device_->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(Type_));
		Capacity_ = num_Descriptors_;

		D3D12_DESCRIPTOR_HEAP_DESC desc_Heap{
			.Type{ static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(Type_) },
			.NumDescriptors{ Capacity_ },
			.Flags{ HeapFlag_ },
		};
		device_->CreateDescriptorHeap(&desc_Heap, IID_PPV_ARGS(GetAddressOf())) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"Failed to create {}!\n",
				debugName_
			)
		};
		Logger().Message<0U>(
			"{} created successfully.\n",
			debugName_
		);

		CPUHandle_Front_ = Wrapped_->GetCPUDescriptorHandleForHeapStart();
		if (HeapFlag_ & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
			GPUHandle_Front_ = Wrapped_->GetGPUDescriptorHandleForHeapStart();
		}

		SetDebugName(debugName_);
	}
}