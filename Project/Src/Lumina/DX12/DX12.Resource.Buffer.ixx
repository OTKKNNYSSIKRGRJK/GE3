module;

#include<d3d12.h>
#include<dxgi1_6.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : Resource.Buffer;

//****	******	******	******	******	****//

import <cstdint>;
import <type_traits>;

import <memory>;

import <vector>;

import <string>;
import <format>;

import : GraphicsDevice;
import : Resource.Commons;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	template<ResourceSettings Settings>
	class CommonBuffer;

	template<ResourceSettings Settings>
		requires(!IsAllocatedInSystemRAM(Settings.HeapProperties))
	class BufferAllocatedInVideoRAM;

	template<ResourceSettings Settings>
		requires(IsAllocatedInSystemRAM(Settings.HeapProperties))
	class BufferAllocatedInSystemRAM;

	class DefaultBuffer;
	class UnorderedAccessBuffer;
	class UploadBuffer;
	class ReadbackBuffer;
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	CommonBuffer							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	class CommonBuffer :
		public Wrapper<CommonBuffer<Settings>, ID3D12Resource>,
		public NonCopyable<CommonBuffer<Settings>>,
		public IBuffer {
		using SelfType = CommonBuffer<Settings>;
		using WrapperType = Wrapper<SelfType, ID3D12Resource>;

		//====	======	======	======	======	====//

	public:
		constexpr auto SizeInBytes() const noexcept -> uint64_t;

		//----	------	------	------	------	----//

	protected:
		auto VerifySize(
			uint64_t sizeInBytes_,
			std::string_view debugName_
		) -> void;
		auto CreateD3D12Resource(
			GraphicsDevice const& device_,
			std::string_view debugName_
		) -> void;

		//----	------	------	------	------	----//

	protected:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t sizeInBytes_,
			std::string_view debugName_
		);

		//----	------	------	------	------	----//

	public:
		constexpr CommonBuffer() noexcept = default;
		constexpr virtual ~CommonBuffer() noexcept = default;

		//====	======	======	======	======	====//

	protected:
		uint64_t SizeInBytes_{};
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	constexpr auto CommonBuffer<Settings>::SizeInBytes() const noexcept ->
		uint64_t {
		return SizeInBytes_;
	}

	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	auto CommonBuffer<Settings>::VerifySize(
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) -> void {
		(sizeInBytes_ > 0LLU) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.CommonBuffer - {}> Size should be larger than zero!\n",
				debugName_
			)
		};
		(sizeInBytes_ < static_cast<uint64_t>(-1)) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.CommonBuffer - {}> Size required for the resource is too large!\n",
				debugName_
			)
		};
		SizeInBytes_ = sizeInBytes_;
	}

	template<ResourceSettings Settings>
	auto CommonBuffer<Settings>::CreateD3D12Resource(
		GraphicsDevice const& device_,
		std::string_view debugName_
	) -> void {
		D3D12_RESOURCE_DESC const resDesc{
			.Dimension{ D3D12_RESOURCE_DIMENSION_BUFFER },
			.Width{ SizeInBytes_ },
			.Height{ 1U },
			.DepthOrArraySize{ 1U },
			.MipLevels{ 1U },
			// D3D12_RESOURCE_DESC::Format must be DXGI_FORMAT_UNKNOWN
			// when D3D12_RESOURCE_DESC::Dimension is D3D12_RESOURCE_DIMENSION_BUFFER 
			.Format{ DXGI_FORMAT_UNKNOWN },
			.SampleDesc{ .Count{ 1U }, },
			.Layout{ D3D12_TEXTURE_LAYOUT_ROW_MAJOR },
			.Flags{ Settings.ResourceFlags },
		};

		device_->CreateCommittedResource(
			&Settings.HeapProperties,
			// When creating a committed resource, D3D12_HEAP_FLAGS must not include
			// D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES,
			// D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES or
			// D3D12_HEAP_FLAG_DENY_BUFFERS.
			// These flags will be set automatically to correspond with the committed resource type.
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			Settings.InitialState,
			nullptr,
			IID_PPV_ARGS(WrapperType::GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommonBuffer> Failed to create {}!\n",
				debugName_
			)
		};
	}

	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	void CommonBuffer<Settings>::Initialize(
		GraphicsDevice const& device_,
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) {
		WrapperType::ThrowIfInitialized(debugName_);

		VerifySize(sizeInBytes_, debugName_);
		CreateD3D12Resource(device_, debugName_);

		WrapperType::SetDebugName(debugName_);
	}
}

//////	//////	//////	//////	//////	//////
//	BufferAllocatedInVideoRAM				//
//	BufferAllocatedInSystemRAM				//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	BufferAllocatedInVideoRAM				//
	//////	//////	//////	//////	//////	//////

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
		requires(!IsAllocatedInSystemRAM(Settings.HeapProperties))
	class BufferAllocatedInVideoRAM :
		public CommonBuffer<Settings> {
		using SelfType = BufferAllocatedInVideoRAM<Settings>;
		using ParentType = CommonBuffer<Settings>;

		//====	======	======	======	======	====//

	protected:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t sizeInBytes_,
			std::string_view debugName_
		);

		//----	------	------	------	------	----//

	public:
		constexpr BufferAllocatedInVideoRAM() noexcept = default;
		constexpr virtual ~BufferAllocatedInVideoRAM() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
		requires(!IsAllocatedInSystemRAM(Settings.HeapProperties))
	void BufferAllocatedInVideoRAM<Settings>::Initialize(
		GraphicsDevice const& device_,
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) {
		ParentType::Initialize(device_, sizeInBytes_, debugName_);
	}

	//////	//////	//////	//////	//////	//////
	//	BufferAllocatedInSystemRAM				//
	//////	//////	//////	//////	//////	//////

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
		requires(IsAllocatedInSystemRAM(Settings.HeapProperties))
	class BufferAllocatedInSystemRAM :
		public CommonBuffer<Settings> {
		using SelfType = BufferAllocatedInSystemRAM<Settings>;
		using ParentType = CommonBuffer<Settings>;

		//====	======	======	======	======	====//

	private:
		void GetCPUPointerToMappedMemory();

		//----	------	------	------	------	----//

	protected:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t sizeInBytes_,
			std::string_view debugName_
		);

		//----	------	------	------	------	----//

	public:
		constexpr BufferAllocatedInSystemRAM() noexcept;
		virtual ~BufferAllocatedInSystemRAM() noexcept;

		//====	======	======	======	======	====//

	protected:
		byte* MappedMemory_{ nullptr };
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
		requires(IsAllocatedInSystemRAM(Settings.HeapProperties))
	void BufferAllocatedInSystemRAM<Settings>::GetCPUPointerToMappedMemory() {
		ParentType::Wrapped_->Map(0U, nullptr, reinterpret_cast<void**>(&MappedMemory_)) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.BufferAllocatedInSystemRAM> Failed to map memory for {}!\n",
				ParentType::DebugName()
			)
		};
	}

	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
		requires(IsAllocatedInSystemRAM(Settings.HeapProperties))
	void BufferAllocatedInSystemRAM<Settings>::Initialize(
		GraphicsDevice const& device_,
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) {
		ParentType::Initialize(device_, sizeInBytes_, debugName_);
		GetCPUPointerToMappedMemory();
	}

	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
		requires(IsAllocatedInSystemRAM(Settings.HeapProperties))
	constexpr BufferAllocatedInSystemRAM<Settings>::
		BufferAllocatedInSystemRAM() noexcept {}

	template<ResourceSettings Settings>
		requires(IsAllocatedInSystemRAM(Settings.HeapProperties))
	BufferAllocatedInSystemRAM<Settings>::
		~BufferAllocatedInSystemRAM() noexcept {
		if (MappedMemory_ != nullptr) {
			ParentType::Wrapped_->Unmap(0, nullptr);
		}
	}
}

//////	//////	//////	//////	//////	//////
//	DefaultBuffer							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings DefaultBufferSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_DEFAULT }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_NONE },
			.InitialState{ D3D12_RESOURCE_STATE_COMMON },
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class DefaultBuffer :
		public BufferAllocatedInVideoRAM<DefaultBufferSettings> {
		using SelfType = DefaultBuffer;
		using ParentType = BufferAllocatedInVideoRAM<DefaultBufferSettings>;

		//====	======	======	======	======	====//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t sizeInBytes_,
			std::string_view debugName_ = "DefaultBuffer"
		);

		//----	------	------	------	------	----//

	public:
		constexpr DefaultBuffer() noexcept = default;
		constexpr virtual ~DefaultBuffer() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	void DefaultBuffer::Initialize(
		GraphicsDevice const& device_,
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) {
		ParentType::Initialize(
			device_,
			sizeInBytes_,
			debugName_
		);
	}
}

//////	//////	//////	//////	//////	//////
//	UnorderedAccessBuffer					//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings UnorderedAccessBufferSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_DEFAULT }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS },
			.InitialState{ D3D12_RESOURCE_STATE_COMMON },
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class UnorderedAccessBuffer :
		public BufferAllocatedInVideoRAM<UnorderedAccessBufferSettings> {
		using SelfType = UnorderedAccessBuffer;
		using ParentType = BufferAllocatedInVideoRAM<UnorderedAccessBufferSettings>;

		//====	======	======	======	======	====//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t sizeInBytes_,
			std::string_view debugName_ = "UnorderedAccessBuffer"
		);

		//----	------	------	------	------	----//

	public:
		constexpr UnorderedAccessBuffer() noexcept = default;
		constexpr virtual ~UnorderedAccessBuffer() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	void UnorderedAccessBuffer::Initialize(
		GraphicsDevice const& device_,
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) {
		ParentType::Initialize(
			device_,
			sizeInBytes_,
			debugName_
		);
	}
}

//////	//////	//////	//////	//////	//////
//	UploadBuffer							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings UploadBufferSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_UPLOAD }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_NONE },
			.InitialState{ D3D12_RESOURCE_STATE_GENERIC_READ },
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class UploadBuffer :
		public BufferAllocatedInSystemRAM<UploadBufferSettings> {
		using SelfType = UploadBuffer;
		using ParentType = BufferAllocatedInSystemRAM<UploadBufferSettings>;

		//====	======	======	======	======	====//

	public:
		constexpr byte* operator()() const noexcept;

		void Store(void const* src_, uint64_t sizeInBytes_, uint64_t offsetInBytes_) noexcept;

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t sizeInBytes_,
			std::string_view debugName_ = "UploadBuffer"
		);

		//----	------	------	------	------	----//

	public:
		constexpr UploadBuffer() noexcept = default;
		constexpr virtual ~UploadBuffer() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	constexpr byte* UploadBuffer::operator()() const noexcept {
		return ParentType::MappedMemory_;
	}

	void UploadBuffer::Store(void const* src_, uint64_t sizeInBytes_, uint64_t offsetInBytes_) noexcept {
		std::memcpy(
			ParentType::MappedMemory_ + offsetInBytes_,
			src_,
			sizeInBytes_
		);
	}

	//----	------	------	------	------	----//

	void UploadBuffer::Initialize(
		GraphicsDevice const& device_,
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) {
		ParentType::Initialize(
			device_,
			sizeInBytes_,
			debugName_
		);
	}
}

//////	//////	//////	//////	//////	//////
//	ReadbackBuffer							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings ReadbackBufferSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_READBACK }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_NONE },
			.InitialState{ D3D12_RESOURCE_STATE_COPY_DEST },
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class ReadbackBuffer :
		public BufferAllocatedInSystemRAM<ReadbackBufferSettings> {
		using SelfType = ReadbackBuffer;
		using ParentType = BufferAllocatedInSystemRAM<ReadbackBufferSettings>;

		//====	======	======	======	======	====//

	public:
		constexpr byte const* operator()() const noexcept;

		void Load(void* dst_, uint64_t sizeInBytes_, uint64_t offsetInBytes_) noexcept;

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t sizeInBytes_,
			std::string_view debugName_ = "ReadbackBuffer"
		);

		//----	------	------	------	------	----//

	public:
		constexpr ReadbackBuffer() noexcept = default;
		constexpr virtual ~ReadbackBuffer() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	constexpr byte const* ReadbackBuffer::operator()() const noexcept {
		return ParentType::MappedMemory_;
	}

	void ReadbackBuffer::Load(void* dst_, uint64_t sizeInBytes_, uint64_t offsetInBytes_) noexcept {
		std::memcpy(
			dst_,
			ParentType::MappedMemory_ + offsetInBytes_,
			sizeInBytes_
		);
	}

	//----	------	------	------	------	----//

	void ReadbackBuffer::Initialize(
		GraphicsDevice const& device_,
		uint64_t sizeInBytes_,
		std::string_view debugName_
	) {
		ParentType::Initialize(
			device_,
			sizeInBytes_,
			debugName_
		);
	}
}