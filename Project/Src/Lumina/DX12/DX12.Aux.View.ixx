export module Lumina.DX12.Aux.View;

//****	******	******	******	******	****//

import <cstdint>;
import <type_traits>;

import <d3d12.h>;

import Lumina.DX12;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	class ConstantBufferView;
	template<typename ElementType>
	class ShaderResourceView;
	template<typename ElementType>
	class UnorderedAccessView;
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	ConstantBufferView						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class ConstantBufferView {
	public:
		class Desc;

		//====	======	======	======	======	====//

	public:
		template<typename BufferType>
		static void Create(
			GraphicsDevice const& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			BufferType const& buf_
		);
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<typename BufferType>
	void ConstantBufferView::Create(
		GraphicsDevice const& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		BufferType const& buffer_
	) {
		Desc const resDesc{ buffer_ };
		device_->CreateConstantBufferView(
			&resDesc,
			cpuHandle_
		);
	}

	//====	======	======	======	======	====//

	export using CBV = ConstantBufferView;

	//****	******	******	******	******	****//

	//====	======	======	======	======	====//
	//	ConstantBufferView::Desc				//
	//====	======	======	======	======	====//

	class ConstantBufferView::Desc :
		public D3D12_CONSTANT_BUFFER_VIEW_DESC {
	public:
		template<typename BufferType>
		constexpr Desc(BufferType const& buf_) noexcept;
	};

	//----	------	------	------	------	----//
	//	Buffer CBV Desc							//
	//----	------	------	------	------	----//

	template<typename BufferType>
	constexpr ConstantBufferView::Desc::Desc(
		BufferType const& buf_
	) noexcept {
		// GPU virtual address of the buffer
		BufferLocation = buf_->GetGPUVirtualAddress();
		// Size, in bytes, of the buffer
		SizeInBytes = static_cast<uint32_t>(buf_.SizeInBytes());
	}
}

//////	//////	//////	//////	//////	//////
//	ShaderResourceView						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//
	
	export template<typename ElementType = void>
	class ShaderResourceView {
	public:
		class Desc;

		//====	======	======	======	======	====//

	public:
		template<typename ResourceType>
		static void Create(
			GraphicsDevice const& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			ResourceType const& res_
		);
		template<DXGI_FORMAT ViewFormat, typename ResourceType>
		static void Create(
			GraphicsDevice const& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			ResourceType const& res_
		);
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<typename ElementType>
	template<typename ResourceType>
	void ShaderResourceView<ElementType>::Create(
		GraphicsDevice const& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		ResourceType const& res_
	) {
		Desc const resDesc{ res_ };
		device_->CreateShaderResourceView(
			res_.Get(),
			&resDesc,
			cpuHandle_
		);
	}

	template<typename ElementType>
	template<DXGI_FORMAT ViewFormat, typename ResourceType>
	void ShaderResourceView<ElementType>::Create(
		GraphicsDevice const& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		ResourceType const& res_
	) {
		Desc const resDesc{ res_, ViewFormat };
		device_->CreateShaderResourceView(
			res_.Get(),
			&resDesc,
			cpuHandle_
		);
	}

	//====	======	======	======	======	====//

	export template<typename ElementType>
	using SRV = ShaderResourceView<ElementType>;
	export using RawSRV = ShaderResourceView<void>;

	//****	******	******	******	******	****//

	//====	======	======	======	======	====//
	//	ShaderResourceView::Desc				//
	//====	======	======	======	======	====//

	template<typename ElementType>
	class ShaderResourceView<ElementType>::Desc :
		public D3D12_SHADER_RESOURCE_VIEW_DESC {
	public:
		template<Concept_Buffer BufferType>
		constexpr Desc(BufferType const& buf_) noexcept;
		template<Concept_Texture2D Tex2DType>
		constexpr Desc(
			Tex2DType const& tex2D_,
			DXGI_FORMAT viewFormat_ = DXGI_FORMAT_UNKNOWN
		) noexcept;
	};

	//----	------	------	------	------	----//
	//	Buffer SRV								//
	//----	------	------	------	------	----//

	template<typename ElementType>
	template<Concept_Buffer BufferType>
	constexpr ShaderResourceView<ElementType>::Desc::Desc(
		BufferType const& buf_
	) noexcept {

		//----	------	------	------	------	----//
		//	Typed									//
		//----	------	------	------	------	----//

		if constexpr (!std::is_void_v<ElementType>) {
			Format = DXGI_FORMAT_UNKNOWN;
			ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			Buffer = D3D12_BUFFER_SRV{
				.FirstElement{ 0LLU },
				.NumElements{ static_cast<uint32_t>(buf_.SizeInBytes() / sizeof(ElementType)) },
				.StructureByteStride{ sizeof(ElementType) },
				.Flags{ D3D12_BUFFER_SRV_FLAG_NONE },
			};
		}

		//----	------	------	------	------	----//
		//	Typeless								//
		//----	------	------	------	------	----//

		else {
			Format = DXGI_FORMAT_R32_TYPELESS;
			ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			Buffer = D3D12_BUFFER_SRV{
				.FirstElement{ 0LLU },
				.NumElements{ static_cast<uint32_t>(buf_.SizeInBytes()) >> 2U },
				.Flags{ D3D12_BUFFER_SRV_FLAG_RAW },
			};
		}
	}

	//----	------	------	------	------	----//
	//	Tex2D SRV								//
	//----	------	------	------	------	----//

	template<typename ElementType>
	template<Concept_Texture2D Tex2DType>
	constexpr ShaderResourceView<ElementType>::Desc::Desc(
		Tex2DType const& tex2D_,
		DXGI_FORMAT viewFormat_
	) noexcept {

		//----	------	------	------	------	----//
		//	Typed									//
		//----	------	------	------	------	----//

		if constexpr (!std::is_void_v<ElementType>) {
		}

		//----	------	------	------	------	----//
		//	Typeless								//
		//----	------	------	------	------	----//

		else {
			Format = (viewFormat_ == DXGI_FORMAT_UNKNOWN) ? (tex2D_.Format()) : (viewFormat_);
			
			// The resource and view creation should be revised!
			if (tex2D_.ArraySize() == 6) {
				ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
				Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				TextureCube = {
					.MostDetailedMip{ 0 },
					.MipLevels{ UINT_MAX },
					.ResourceMinLODClamp{ 0.0f },
				};
			}
			else if (tex2D_.ArraySize() == 1) {
				ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				Texture2D = D3D12_TEX2D_SRV{
					.MipLevels{ static_cast<uint32_t>(tex2D_.MipLevels()) },
				};
			}
		}
	}
}

//////	//////	//////	//////	//////	//////
//	UnorderedAccessView						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	
	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export template<typename ElementType = void>
	class UnorderedAccessView {
	public:
		class Desc;

		//====	======	======	======	======	====//

	public:
		static void Create(
			const GraphicsDevice& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			const UnorderedAccessBuffer& buffer_
		);
		static void Create(
			const GraphicsDevice& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			const UnorderedAccessBuffer& buffer_,
			const UnorderedAccessBuffer& counter_
		);

		static void Create(
			const GraphicsDevice& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			const ComputeTexture2D& tex2D_
		);
		static void Create(
			const GraphicsDevice& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			const RenderComputeTexture2D& tex2D_
		);
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<typename ElementType>
	void UnorderedAccessView<ElementType>::Create(
		const GraphicsDevice& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		const UnorderedAccessBuffer& buffer_
	) {
		const Desc desc{ buffer_ };
		device_->CreateUnorderedAccessView(
			buffer_.Get(),
			nullptr,
			&desc,
			cpuHandle_
		);
	}

	template<typename ElementType>
	void UnorderedAccessView<ElementType>::Create(
		const GraphicsDevice& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		const UnorderedAccessBuffer& buffer_,
		const UnorderedAccessBuffer& counter_
	) {
		Desc uavDesc{ buffer_ };
		device_->CreateUnorderedAccessView(
			buffer_.Get(),
			counter_.Get(),
			&uavDesc,
			cpuHandle_
		);
	}

	template<typename ElementType>
	void UnorderedAccessView<ElementType>::Create(
		const GraphicsDevice& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		const ComputeTexture2D& tex2D_
	) {
		Desc uavDesc{ tex2D_ };
		device_->CreateUnorderedAccessView(
			tex2D_.Get(),
			nullptr,
			&uavDesc,
			cpuHandle_
		);
	}

	template<typename ElementType>
	void UnorderedAccessView<ElementType>::Create(
		const GraphicsDevice& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		const RenderComputeTexture2D& tex2D_
	) {
		Desc uavDesc{ tex2D_ };
		device_->CreateUnorderedAccessView(
			tex2D_.Get(),
			nullptr,
			&uavDesc,
			cpuHandle_
		);
	}

	//====	======	======	======	======	====//

	export template<typename ElementType>
	using UAV = UnorderedAccessView<ElementType>;
	export using RawUAV = UnorderedAccessView<void>;

	//****	******	******	******	******	****//

	//====	======	======	======	======	====//
	//	UnorderedAccessView::Desc				//
	//====	======	======	======	======	====//

	template<typename ElementType>
	class UnorderedAccessView<ElementType>::Desc :
		public D3D12_UNORDERED_ACCESS_VIEW_DESC {
	public:
		constexpr Desc(UnorderedAccessBuffer const& buf_) noexcept;
		constexpr Desc(ComputeTexture2D const& tex2D_) noexcept;
		constexpr Desc(RenderComputeTexture2D const& tex2D_) noexcept;
	};

	//----	------	------	------	------	----//
	//	Buffer UAV								//
	//----	------	------	------	------	----//

	template<typename ElementType>
	constexpr UnorderedAccessView<ElementType>::Desc::Desc(
		UnorderedAccessBuffer const& buf_
	) noexcept {

		//----	------	------	------	------	----//
		//	Typed									//
		//----	------	------	------	------	----//

		if constexpr (!std::is_void_v<ElementType>) {
			Format = DXGI_FORMAT_UNKNOWN;
			ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			Buffer = D3D12_BUFFER_UAV{
				.FirstElement{ 0LLU },
				.NumElements{ static_cast<uint32_t>(buf_.SizeInBytes() / sizeof(ElementType)) },
				.StructureByteStride{ static_cast<uint32_t>(sizeof(ElementType)) },
				.CounterOffsetInBytes{ 0U },
				.Flags{ D3D12_BUFFER_UAV_FLAG_NONE },
			};
		}

		//----	------	------	------	------	----//
		//	Typeless								//
		//----	------	------	------	------	----//

		else {
			// The format of a UAV with D3D12_BUFFER_UAV_FLAG_RAW must be DXGI_FORMAT_R32_TYPELESS.
			Format = DXGI_FORMAT_R32_TYPELESS;
			ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			Buffer = D3D12_BUFFER_UAV{
				.FirstElement{ 0LLU },
				.NumElements{ static_cast<uint32_t>(buf_.SizeInBytes()) >> 2U },
				.Flags{ D3D12_BUFFER_UAV_FLAG_RAW },
			};
		}
	}

	//----	------	------	------	------	----//
	//	Tex2D UAV								//
	//----	------	------	------	------	----//

	template<typename ElementType>
	constexpr UnorderedAccessView<ElementType>::Desc::Desc(
		ComputeTexture2D const& tex2D_
	) noexcept {

		//----	------	------	------	------	----//
		//	Typed									//
		//----	------	------	------	------	----//

		if constexpr (!std::is_void_v<ElementType>) {
			Format = DXGI_FORMAT_UNKNOWN;
			ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			Texture2D = D3D12_TEX2D_UAV{};
		}

		//----	------	------	------	------	----//
		//	Typeless								//
		//----	------	------	------	------	----//

		else {
			Format = tex2D_.Format();
			ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			Texture2D = D3D12_TEX2D_UAV{};
		}
	}

	template<typename ElementType>
	constexpr UnorderedAccessView<ElementType>::Desc::Desc(
		RenderComputeTexture2D const& tex2D_
	) noexcept {

		//----	------	------	------	------	----//
		//	Typed									//
		//----	------	------	------	------	----//

		if constexpr (!std::is_void_v<ElementType>) {
			Format = DXGI_FORMAT_UNKNOWN;
			ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			Texture2D = D3D12_TEX2D_UAV{};
		}

		//----	------	------	------	------	----//
		//	Typeless								//
		//----	------	------	------	------	----//

		else {
			Format = tex2D_.Format();
			ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			Texture2D = D3D12_TEX2D_UAV{};
		}
	}
}

//////	//////	//////	//////	//////	//////
//	RenderTargetView						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class RenderTargetView {
	public:
		class Desc;

		//====	======	======	======	======	====//

	public:
		template<Concept_Resource ResourceType>
		static void Create(
			GraphicsDevice const& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			ResourceType const& res_
		);
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<Concept_Resource ResourceType>
	void RenderTargetView::Create(
		GraphicsDevice const& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		ResourceType const& res_
	) {
		Desc const resDesc{ res_ };
		device_->CreateRenderTargetView(
			res_.Get(),
			&resDesc,
			cpuHandle_
		);
	}

	//====	======	======	======	======	====//

	export using RTV = RenderTargetView;

	//****	******	******	******	******	****//

	//====	======	======	======	======	====//
	//	RenderTargetView::Desc					//
	//====	======	======	======	======	====//

	class RenderTargetView::Desc :
		public D3D12_RENDER_TARGET_VIEW_DESC {
	public:
		template<Concept_Texture2D Tex2DType>
		constexpr Desc(Tex2DType const& tex2D_) noexcept;
	};

	//----	------	------	------	------	----//
	//	Tex2D RTV								//
	//----	------	------	------	------	----//

	template<Concept_Texture2D Tex2DType>
	constexpr RenderTargetView::Desc::Desc(
		Tex2DType const& tex2D_
	) noexcept {
		Format = tex2D_.Format();
		ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		Texture2D = D3D12_TEX2D_RTV{};
	}
}

//////	//////	//////	//////	//////	//////
//	DepthStencilView						//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class DepthStencilView {
	public:
		class Desc;

		//====	======	======	======	======	====//

	public:
		template<Concept_Resource ResourceType>
		static void Create(
			GraphicsDevice const& device_,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
			ResourceType const& res_,
			DXGI_FORMAT format_
		);
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	template<Concept_Resource ResourceType>
	void DepthStencilView::Create(
		GraphicsDevice const& device_,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_,
		ResourceType const& res_,
		DXGI_FORMAT format_
	) {
		Desc const resDesc{ res_, format_ };
		device_->CreateDepthStencilView(
			res_.Get(),
			&resDesc,
			cpuHandle_
		);
	}

	//====	======	======	======	======	====//

	export using DSV = DepthStencilView;

	//****	******	******	******	******	****//

	//====	======	======	======	======	====//
	//	DepthStencilView::Desc					//
	//====	======	======	======	======	====//

	class DepthStencilView::Desc :
		public D3D12_DEPTH_STENCIL_VIEW_DESC {
	public:
		constexpr Desc(
			[[maybe_unused]] DepthTexture2D const& tex2D_,
			DXGI_FORMAT format_
		) noexcept;
	};

	//----	------	------	------	------	----//
	//	Tex2D DSV								//
	//----	------	------	------	------	----//

	constexpr DepthStencilView::Desc::Desc(
		[[maybe_unused]] DepthTexture2D const& tex2D_,
		DXGI_FORMAT format_
	) noexcept {
		Format = format_;
		ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		Flags = D3D12_DSV_FLAG_NONE;
		Texture2D = D3D12_TEX2D_DSV{};
	}
}

//////	//////	//////	//////	//////	//////
//	VertexBufferView						//
//	IndexBufferView							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	VertexBufferView						//
	//////	//////	//////	//////	//////	//////

	export class VertexBufferView : public D3D12_VERTEX_BUFFER_VIEW {
	public:
		template<typename VertexType, typename BufferType>
		static constexpr VertexBufferView Create(BufferType const& buf_) noexcept {
			D3D12_VERTEX_BUFFER_VIEW vbv{
				// GPU virtual address of the buffer
				.BufferLocation{ buf_->GetGPUVirtualAddress() },
				// Size, in bytes, of the buffer
				.SizeInBytes{ static_cast<uint32_t>(buf_.SizeInBytes()) },
				// Size, in bytes, per vertex
				.StrideInBytes{ sizeof(VertexType) },
			};
			return static_cast<VertexBufferView>(vbv);
		}
	};

	//====	======	======	======	======	====//

	export using VBV = VertexBufferView;

	//****	******	******	******	******	****//

	//////	//////	//////	//////	//////	//////
	//	IndexBufferView							//
	//////	//////	//////	//////	//////	//////
	
	export class IndexBufferView : public D3D12_INDEX_BUFFER_VIEW {
	public:
		template<typename BufferType>
		static constexpr IndexBufferView Create(BufferType const& buf_) noexcept {
			D3D12_INDEX_BUFFER_VIEW ibv{
				.BufferLocation{ buf_->GetGPUVirtualAddress() },
				.SizeInBytes{ static_cast<uint32_t>(buf_.SizeInBytes()) },
				.Format{ DXGI_FORMAT_R32_UINT },
			};
			return static_cast<IndexBufferView>(ibv);
		}
	};

	//====	======	======	======	======	====//

	export using IBV = IndexBufferView;
}