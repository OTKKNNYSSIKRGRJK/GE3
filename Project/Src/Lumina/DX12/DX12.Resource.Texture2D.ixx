module;

#include<d3d12.h>
#include<dxgi1_6.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : Resource.Texture2D;

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

import Lumina.Math.Numerics;

import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	template<ResourceSettings Settings>
	class CommonTexture2D;

	class DefaultTexture2D;
	class RenderTexture2D;
	class ComputeTexture2D;
	class DepthStencilTexture2D;
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	CommonTexture2D							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	class CommonTexture2D :
		public Wrapper<CommonTexture2D<Settings>, ID3D12Resource>,
		public NonCopyable<CommonTexture2D<Settings>>,
		public ITexture2D {
		using SelfType = CommonTexture2D<Settings>;
		using WrapperType = Wrapper<SelfType, ID3D12Resource>;

		//====	======	======	======	======	====//

	public:
		constexpr auto Width() const noexcept -> uint64_t;
		constexpr auto Height() const noexcept -> uint32_t;
		constexpr auto MipLevels() const noexcept -> uint32_t;
		constexpr auto Format() const noexcept -> DXGI_FORMAT;
		constexpr auto Flags() const noexcept -> D3D12_RESOURCE_FLAGS;

		//----	------	------	------	------	----//

	protected:
		auto VerifySize(
			uint64_t width_,
			uint32_t height_,
			std::string_view debugName_
		) -> void;
		auto GenerateResourceDesc(
			uint16_t mipLevels_,
			DXGI_FORMAT format_,
			DXGI_SAMPLE_DESC const& sampleDesc_
		) -> D3D12_RESOURCE_DESC;
		virtual auto CreateD3D12Resource(
			GraphicsDevice const& device_,
			D3D12_RESOURCE_DESC const& resDesc_,
			std::string_view debugName_
		) -> void;

		//----	------	------	------	------	----//

	protected:
		void Initialize(
			GraphicsDevice const& device_,
			uint64_t width_,
			uint32_t height_,
			uint16_t mipLevels_,
			DXGI_FORMAT format_,
			DXGI_SAMPLE_DESC const& sampleDesc_,
			std::string_view debugName_ = "Tex2D"
		);

		//----	------	------	------	------	----//

	public:
		constexpr CommonTexture2D() noexcept = default;
		constexpr virtual ~CommonTexture2D() noexcept = default;

		//====	======	======	======	======	====//

	protected:
		uint64_t Width_{};
		uint32_t Height_{};

		uint32_t MipLevels_{};
		DXGI_FORMAT Format_{};
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	constexpr auto CommonTexture2D<Settings>::Width()
		const noexcept -> uint64_t {
		return Width_;
	}
	template<ResourceSettings Settings>
	constexpr auto CommonTexture2D<Settings>::Height()
		const noexcept -> uint32_t {
		return Height_;
	}
	template<ResourceSettings Settings>
	constexpr auto CommonTexture2D<Settings>::MipLevels()
		const noexcept -> uint32_t {
		return MipLevels_;
	}
	template<ResourceSettings Settings>
	constexpr auto CommonTexture2D<Settings>::Format()
		const noexcept -> DXGI_FORMAT {
		return Format_;
	}
	template<ResourceSettings Settings>
	constexpr auto CommonTexture2D<Settings>::Flags()
		const noexcept -> D3D12_RESOURCE_FLAGS {
		return Settings.ResourceFlags;
	}

	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	auto CommonTexture2D<Settings>::VerifySize(
		uint64_t width_,
		uint32_t height_,
		std::string_view debugName_
	) -> void {
		(width_ && height_) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.CommonTexture2D - {}> Both width and height must be nonzero!\n",
				debugName_
			)
		};
		Width_ = width_;
		Height_ = height_;
	}

	template<ResourceSettings Settings>
	auto CommonTexture2D<Settings>::GenerateResourceDesc(
		uint16_t mipLevels_,
		DXGI_FORMAT format_,
		DXGI_SAMPLE_DESC const& sampleDesc_
	) -> D3D12_RESOURCE_DESC {
		MipLevels_ = mipLevels_;
		Format_ = format_;

		return D3D12_RESOURCE_DESC{
			.Dimension{ D3D12_RESOURCE_DIMENSION_TEXTURE2D },
			.Width{ Width_ },
			.Height{ Height_ },
			.DepthOrArraySize{ 1U },
			.MipLevels{ static_cast<uint16_t>(MipLevels_) },
			.Format{ Format_ },
			.SampleDesc{ sampleDesc_ },
			.Flags{ Settings.ResourceFlags },
		};
	}

	template<ResourceSettings Settings>
	auto CommonTexture2D<Settings>::CreateD3D12Resource(
		GraphicsDevice const& device_,
		D3D12_RESOURCE_DESC const& resDesc_,
		std::string_view debugName_
	) -> void {
		device_->CreateCommittedResource(
			&Settings.HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resDesc_,
			Settings.InitialState,
			nullptr,
			IID_PPV_ARGS(WrapperType::GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.CommonTexture2D> Failed to create {}!\n",
				debugName_
			)
		};
	}

	//----	------	------	------	------	----//

	template<ResourceSettings Settings>
	void CommonTexture2D<Settings>::Initialize(
		GraphicsDevice const& device_,
		uint64_t width_,
		uint32_t height_,
		uint16_t mipLevels_,
		DXGI_FORMAT format_,
		DXGI_SAMPLE_DESC const& sampleDesc_,
		std::string_view debugName_
	) {
		WrapperType::ThrowIfInitialized(debugName_);

		VerifySize(width_, height_, debugName_);
		auto&& resDesc{ GenerateResourceDesc(mipLevels_, format_, sampleDesc_) };
		CreateD3D12Resource(device_, resDesc, debugName_);

		WrapperType::SetDebugName(debugName_);
	}
}

//////	//////	//////	//////	//////	//////
//	DefaultTexture2D						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings DefaultTexture2DSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_DEFAULT }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_NONE },
			.InitialState{ D3D12_RESOURCE_STATE_COPY_DEST },
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class DefaultTexture2D :
		public CommonTexture2D<DefaultTexture2DSettings> {
		using ParentType = CommonTexture2D<DefaultTexture2DSettings>;

		//====	======	======	======	======	====//

	public:
		virtual void Initialize(
			GraphicsDevice const& device_,
			uint32_t width_,
			uint32_t height_,
			uint16_t mipLevels_,
			DXGI_FORMAT format_,
			std::string_view debugName_ = "DefaultTex2D"
		);

		//----	------	------	------	------	----//

	public:
		constexpr DefaultTexture2D() noexcept = default;
		constexpr virtual ~DefaultTexture2D() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	void DefaultTexture2D::Initialize(
		GraphicsDevice const& device_,
		uint32_t width_,
		uint32_t height_,
		uint16_t mipLevels_,
		DXGI_FORMAT format_,
		std::string_view debugName_
	) {
		ParentType::Initialize(
			device_,
			width_,
			height_,
			mipLevels_,
			format_,
			SampleDesc_NoMultisampling,
			debugName_
		);
	}
}

//////	//////	//////	//////	//////	//////
//	RenderTexture2D							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings RenderTexture2DSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_DEFAULT }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET },
			.InitialState{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class RenderTexture2D :
		public CommonTexture2D<RenderTexture2DSettings> {
		using ParentType = CommonTexture2D<RenderTexture2DSettings>;

		//====	======	======	======	======	====//

	private:
		auto CreateD3D12Resource(
			GraphicsDevice const& device_,
			D3D12_RESOURCE_DESC const& resDesc_,
			Float4 const& clearColor_,
			std::string_view debugName_
		) -> void;

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint32_t width_,
			uint32_t height_,
			DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			Float4 const& clearColor_ = { 0.0f, 0.0f, 0.0f, 1.0f },
			std::string_view debugName_ = "RenderTex2D"
		);

		//----	------	------	------	------	----//

	public:
		constexpr RenderTexture2D() noexcept = default;
		constexpr virtual ~RenderTexture2D() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	auto RenderTexture2D::CreateD3D12Resource(
		GraphicsDevice const& device_,
		D3D12_RESOURCE_DESC const& resDesc_,
		Float4 const& clearColor_,
		std::string_view debugName_
	) -> void {
		D3D12_CLEAR_VALUE clearValue{
			.Format{ resDesc_.Format },
			.Color{ clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w, },
		};

		device_->CreateCommittedResource(
			&RenderTexture2DSettings.HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resDesc_,
			RenderTexture2DSettings.InitialState,
			&clearValue,
			IID_PPV_ARGS(GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.RenderTexture2D> Failed to create {}!\n",
				debugName_
			)
		};
	}

	//----	------	------	------	------	----//

	void RenderTexture2D::Initialize(
		GraphicsDevice const& device_,
		uint32_t width_,
		uint32_t height_,
		DXGI_FORMAT format_,
		Float4 const& clearColor_,
		std::string_view debugName_
	) {
		ParentType::ThrowIfInitialized(debugName_);

		ParentType::VerifySize(width_, height_, debugName_);
		auto&& resDesc{
			ParentType::GenerateResourceDesc(
				1U,
				format_,
				SampleDesc_NoMultisampling
			)
		};
		CreateD3D12Resource(device_, resDesc, clearColor_, debugName_);

		ParentType::SetDebugName(debugName_);
	}
}

//////	//////	//////	//////	//////	//////
//	ComputeTexture2D						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings ComputeTexture2DSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_DEFAULT }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS },
			.InitialState{
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			},
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class ComputeTexture2D :
		public CommonTexture2D<ComputeTexture2DSettings> {
		using ParentType = CommonTexture2D<ComputeTexture2DSettings>;

		//====	======	======	======	======	====//

	private:
		auto CreateD3D12Resource(
			GraphicsDevice const& device_,
			D3D12_RESOURCE_DESC const& resDesc_,
			std::string_view debugName_
		) -> void;

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint32_t width_,
			uint32_t height_,
			DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM,
			std::string_view debugName_ = "ComputeTex2D"
		);

		//----	------	------	------	------	----//

	public:
		constexpr ComputeTexture2D() noexcept = default;
		constexpr virtual ~ComputeTexture2D() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	auto ComputeTexture2D::CreateD3D12Resource(
		GraphicsDevice const& device_,
		D3D12_RESOURCE_DESC const& resDesc_,
		std::string_view debugName_
	) -> void {
		device_->CreateCommittedResource(
			&ComputeTexture2DSettings.HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resDesc_,
			ComputeTexture2DSettings.InitialState,
			nullptr,
			IID_PPV_ARGS(GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.ComputeTexture2D> Failed to create {}!\n",
				debugName_
			)
		};
	}

	//----	------	------	------	------	----//

	void ComputeTexture2D::Initialize(
		GraphicsDevice const& device_,
		uint32_t width_,
		uint32_t height_,
		DXGI_FORMAT format_,
		std::string_view debugName_
	) {
		ParentType::ThrowIfInitialized(debugName_);

		ParentType::VerifySize(width_, height_, debugName_);
		auto&& resDesc{
			ParentType::GenerateResourceDesc(
				1U,
				format_,
				SampleDesc_NoMultisampling
			)
		};
		CreateD3D12Resource(device_, resDesc, debugName_);

		ParentType::SetDebugName(debugName_);
	}
}


//////	//////	//////	//////	//////	//////
//	DepthStencilTexture2D					//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Settings								//
	//----	------	------	------	------	----//

	namespace {
		constexpr ResourceSettings DepthStencilTexture2DSettings{
			.HeapProperties{ .Type{ D3D12_HEAP_TYPE_DEFAULT }, },
			.ResourceFlags{ D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL },
			.InitialState{ D3D12_RESOURCE_STATE_DEPTH_WRITE },
		};
	}

	//****	******	******	******	******	****//

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class DepthStencilTexture2D :
		public CommonTexture2D<DepthStencilTexture2DSettings> {
		using ParentType = CommonTexture2D<DepthStencilTexture2DSettings>;

		//====	======	======	======	======	====//

	private:
		virtual auto CreateD3D12Resource(
			GraphicsDevice const& device_,
			D3D12_RESOURCE_DESC const& resDesc_,
			std::string_view debugName_
		) -> void override;

		//----	------	------	------	------	----//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			uint32_t width_,
			uint32_t height_,
			std::string_view debugName_ = "DSTex"
		);

		//----	------	------	------	------	----//

	public:
		constexpr DepthStencilTexture2D() noexcept = default;
		constexpr virtual ~DepthStencilTexture2D() noexcept = default;
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	auto DepthStencilTexture2D::CreateD3D12Resource(
		GraphicsDevice const& device_,
		D3D12_RESOURCE_DESC const& resDesc_,
		std::string_view debugName_
	) -> void {
		D3D12_CLEAR_VALUE clearValue{
			// Same format as of the resource
			.Format{ resDesc_.Format },
			// Clears the depth stencil texture by the largest valid value (i.e. 1.0f).
			.DepthStencil{ .Depth{ 1.0f } },
		};

		device_->CreateCommittedResource(
			&DepthStencilTexture2DSettings.HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resDesc_,
			DepthStencilTexture2DSettings.InitialState,
			&clearValue,
			IID_PPV_ARGS(GetAddressOf())
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.DepthStencilTexture2D> Failed to create {}!\n",
				debugName_
			)
		};
	}

	//----	------	------	------	------	----//

	void DepthStencilTexture2D::Initialize(
		GraphicsDevice const& device_,
		uint32_t width_,
		uint32_t height_,
		std::string_view debugName_
	) {
		ParentType::ThrowIfInitialized(debugName_);

		ParentType::VerifySize(width_, height_, debugName_);
		auto&& resDesc{
			ParentType::GenerateResourceDesc(
				1U,
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				SampleDesc_NoMultisampling
			)
		};
		CreateD3D12Resource(device_, resDesc, debugName_);

		ParentType::SetDebugName(debugName_);
	}
}