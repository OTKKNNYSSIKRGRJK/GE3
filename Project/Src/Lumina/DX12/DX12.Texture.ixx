module;

#include<d3d12.h>

// DirectXTex.h			| Image IO
// DirectXTex.lib		| Implementation of DirectXTex.h
#include<External/DirectXTex/d3dx12.h>
#include<External/DirectXTex/DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : ImageTexture;

import <cstdint>;

import <vector>;

import <memory>;

import <thread>;
import <future>;

import <string>;
import <format>;

import : GraphicsDevice;
import : Command;
import : Descriptor;
import : Resource;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.Utils.String;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	class ImageSet;
	class MipChain;

	class ImageTexture;
	class ImageTextureUploader;
}

//////	//////	//////	//////	//////	//////

export namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	ImageSet								//
	//////	//////	//////	//////	//////	//////

	class ImageSet : private DirectX::ScratchImage {
	public:
		inline const DirectX::Image* operator()() const noexcept { return GetImages(); }

		inline size_t Count() const noexcept { return GetImageCount(); }
		inline const DirectX::TexMetadata& Metadata() const noexcept { return GetMetadata(); }

		//----	------	------	------	------	----//

	public:
		[[nodiscard]] static UniPtr<ImageSet> Create(
			std::string_view filePath_,
			DirectX::WIC_FLAGS flags_ = DirectX::WIC_FLAGS_FORCE_SRGB
		);

		//----	------	------	------	------	----//

	private:
		ImageSet(
			std::string_view filePath_,
			DirectX::WIC_FLAGS flags_
		);
	public:
		virtual ~ImageSet() noexcept;
	};

	//////	//////	//////	//////	//////	//////
	//	MipChain								//
	//////	//////	//////	//////	//////	//////

	class MipChain : private DirectX::ScratchImage {
	public:
		inline const DirectX::Image* operator()() const noexcept { return GetImages(); }

		inline size_t Count() const noexcept { return GetImageCount(); }
		inline const DirectX::TexMetadata& Metadata() const noexcept { return GetMetadata(); }

		//----	------	------	------	------	----//

	public:
		[[nodiscard]] static UniPtr<MipChain> Create(
			const ImageSet& imgSet_,
			DirectX::TEX_FILTER_FLAGS flags_TexFilter_ = DirectX::TEX_FILTER_SRGB
		);

		//----	------	------	------	------	----//

	private:
		MipChain(
			const ImageSet& imgSet_,
			DirectX::TEX_FILTER_FLAGS flags_TexFilter_
		);
	public:
		virtual ~MipChain() noexcept;
	};
}

export namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	ImageTexture							//
	//////	//////	//////	//////	//////	//////

	class ImageTexture :
		public DefaultTexture2D {
		friend ImageTextureUploader;

		//====	======	======	======	======	====//

	private:
		class SubresourceMetadata;
		class ResourceMetadata;
		class Intermediate;

		//====	======	======	======	======	====//

	public:
		enum class STATUS {
			READY_TO_UPLOAD,
			UPLOAD_IN_PROGRESS,
			READY_TO_USE,
		};

		//====	======	======	======	======	====//

	public:
		inline D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc() const noexcept;

		inline uint32_t Width() const noexcept;
		inline uint32_t Height() const noexcept;

		inline STATUS Status() const noexcept;
		inline const Intermediate& IntermediateData() const noexcept;

		//----	------	------	------	------	----//

	private:
		inline void CreateIntermediateData(
			const GraphicsDevice& device_,
			const MipChain& mipChain_
		);
		inline void ReleaseIntermediateData() noexcept;

		//----	------	------	------	------	----//

	public:
		void Initialize(
			const GraphicsDevice& device_,
			const MipChain& mipChain_,
			std::string_view debugName_ = "ImageTexture"
		);

		//----	------	------	------	------	----//

	public:
		// Creates on the default heap a 2D texture of which description is based on the metadata of the mip chain.
		// Intermediate data is created as well, which is supposed to be deleted 
		[[nodiscard]] static UniPtr<ImageTexture> Create(
			GraphicsDevice const& device_,
			MipChain const& mipChain_,
			std::string_view debugName_
		);

		//----	------	------	------	------	----//

	public:
		constexpr ImageTexture() noexcept;
		virtual ~ImageTexture() noexcept;

		//====	======	======	======	======	====//

	private:
		D3D12_RESOURCE_DESC ResourceDesc_{};

		STATUS Status_{ STATUS::READY_TO_UPLOAD };
		Intermediate* IntermediateData_{ nullptr };
	};

	//////	//////	//////	//////	//////	//////
	//	ImageTextureUploader					//
	//////	//////	//////	//////	//////	//////

	class ImageTextureUploader : public NonCopyable<ImageTextureUploader> {
	public:
		inline ImageTextureUploader& operator<<(ImageTexture& tex_);

		//----	------	------	------	------	----//

	public:
		bool Begin();
		std::future<void> End(CommandQueue& cmdQueue_);
		void Batch(ImageTexture& tex_);

		//----	------	------	------	------	----//

	private:
		void ClearBatch() noexcept;

		//----	------	------	------	------	----//

	public:
		void Initialize(GraphicsDevice const& device_);

		//----	------	------	------	------	----//

	public:
		ImageTextureUploader() noexcept;
		~ImageTextureUploader() noexcept;

		//====	======	======	======	======	====//

	private:
		std::vector<ImageTexture*> BatchedTextures_{};
		std::vector<D3D12_RESOURCE_BARRIER> BatchedResourceBarriers_{};

		CommandAllocator CommandAllocator_{};
		CommandList CommandList_{};

		std::thread Thread_ClearBatch_{};

		int32_t IsInBeginEndBlock_{ 0 };
	};
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	ImageSet								//
//	MipChain								//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	ImageSet								//
	//////	//////	//////	//////	//////	//////

	UniPtr<ImageSet> ImageSet::Create(
		std::string_view filePath_,
		DirectX::WIC_FLAGS flags_
	) {
		return UniPtr<ImageSet>{
			new ImageSet{ filePath_, flags_ }
		};
	}

	//----	------	------	------	------	----//

	ImageSet::ImageSet(std::string_view filePath_, DirectX::WIC_FLAGS flags_) {
		DirectX::LoadFromWICFile(
			Utils::String::Convert(filePath_).data(),
			flags_,
			nullptr,
			static_cast<DirectX::ScratchImage&>(*this)
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format("<DX12.ImageSet> Failed to load \"{}\"!\n", filePath_)
		};
		Logger().Message<0U>(
			"ImageSet,\"{}\",Image loaded successfully.\n",
			filePath_
		);
	}

	ImageSet::~ImageSet() noexcept {}

	//////	//////	//////	//////	//////	//////
	//	MipChain								//
	//////	//////	//////	//////	//////	//////

	UniPtr<MipChain> MipChain::Create(
		const ImageSet& imgSet_,
		DirectX::TEX_FILTER_FLAGS flags_TexFilter_
	) {
		return UniPtr<MipChain>{
			new MipChain{ imgSet_, flags_TexFilter_ }
		};
	}

	//----	------	------	------	------	----//

	MipChain::MipChain(
		const ImageSet& imgSet_,
		DirectX::TEX_FILTER_FLAGS flags_TexFilter_
	) {
		DirectX::GenerateMipMaps(
			imgSet_(),
			imgSet_.Count(),
			imgSet_.Metadata(),
			flags_TexFilter_,
			0LLU,
			static_cast<DirectX::ScratchImage&>(*this)
		) ||
		Utils::Debug::ThrowIfFailed{
			"<DX12.MipChain> Failed to generate mipmaps!\n"
		};
		Logger().Message<0U>(
			"MipChain,,Mip chain generated successfully.\n"
		);
	}

	MipChain::~MipChain() noexcept {}
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	ImageTexture::SubresourceMetadata		//
//	ImageTexture::ResourceMetadata			//
//	ImageTexture::Intermediate				//
//////	//////	//////	//////	//////	//////

//----	------	------	------	------	----//
//	Declaration								//
//----	------	------	------	------	----//

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	ImageTexture::SubresourceMetadata		//
	//////	//////	//////	//////	//////	//////

	class ImageTexture::SubresourceMetadata final {
		friend ResourceMetadata;

		//====	======	======	======	======	====//

	public:
		inline const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& PlacedFootprint() const noexcept;
		inline const D3D12_SUBRESOURCE_FOOTPRINT& Footprint() const noexcept;
		inline uint64_t Offset() const noexcept;
		inline uint64_t RowSizeInBytes() const noexcept;
		inline uint32_t Num_Rows() const noexcept;

		//----	------	------	------	------	----//

	private:
		SubresourceMetadata(const ResourceMetadata* resMetadata_, uint32_t idx_Subres_);
	public:
		~SubresourceMetadata() noexcept;

		//====	======	======	======	======	====//

	private:
		const ResourceMetadata* ResourceMetadata_{ nullptr };
		uint32_t Index_Subresource_{};
	};

	//////	//////	//////	//////	//////	//////
	//	ImageTexture::ResourceMetadata			//
	//////	//////	//////	//////	//////	//////

	class ImageTexture::ResourceMetadata {
		friend SubresourceMetadata;
		friend Intermediate;

		//====	======	======	======	======	====//

	public:
		inline SubresourceMetadata operator[](uint32_t idx_Subres_) const noexcept;

		inline uint64_t SizeInBytes_IntermediateData() const noexcept;

		//----	------	------	------	------	----//

	private:
		ResourceMetadata(
			const GraphicsDevice& device_,
			const ImageTexture& tex_,
			const std::vector<D3D12_SUBRESOURCE_DATA>& subresources_
		);
	public:
		~ResourceMetadata() noexcept;

		//====	======	======	======	======	====//

	private:
		HANDLE ProcessHeap_{ nullptr };

		void* Memory_{ nullptr };
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT* PlacedSubresourceFootprints_{ nullptr };
		uint64_t* RowSizesInBytes_{ nullptr };
		uint32_t* Nums_Rows_{ nullptr };

		uint64_t SizeInBytes_IntermediateData_{ 0LLU };
	};

	//////	//////	//////	//////	//////	//////
	//	ImageTexture::Intermediate				//
	//////	//////	//////	//////	//////	//////

	class ImageTexture::Intermediate final {
		friend ImageTexture;

		//====	======	======	======	======	====//

	public:
		inline SubresourceMetadata Metadata(uint32_t idx_Subres_) const noexcept;
		inline const ResourceMetadata& Metadata() const noexcept;
		inline const UploadBuffer& Buffer() const noexcept;
		inline uint32_t Num_Subresources() const noexcept;

		//----	------	------	------	------	----//

	private:
		void GenerateSubresourceData(const GraphicsDevice& device_, const MipChain& mipChain_);
		void AnalyzeSubresourceData(const GraphicsDevice& device_);
		void CopySubresourceData(const GraphicsDevice& device_);

		//----	------	------	------	------	----//

	private:
		Intermediate(
			const GraphicsDevice& device_,
			const ImageTexture& tex_,
			const MipChain& mipChain_
		);
	public:
		~Intermediate() noexcept;

		//====	======	======	======	======	====//

	private:
		const ImageTexture* ImageTexture_{ nullptr };

		std::vector<D3D12_SUBRESOURCE_DATA> Subresources_{};
		UniPtr<ResourceMetadata> Metadata_{ nullptr };
		UploadBuffer Buffer_{};
	};
}

//----	------	------	------	------	----//
//	Implementation							//
//----	------	------	------	------	----//

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	ImageTexture::SubresourceMetadata		//
	//////	//////	//////	//////	//////	//////

	inline const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& ImageTexture::SubresourceMetadata::PlacedFootprint() const noexcept {
		return ResourceMetadata_->PlacedSubresourceFootprints_[Index_Subresource_];
	}

	inline const D3D12_SUBRESOURCE_FOOTPRINT& ImageTexture::SubresourceMetadata::Footprint() const noexcept {
		return ResourceMetadata_->PlacedSubresourceFootprints_[Index_Subresource_].Footprint;
	}

	inline uint64_t ImageTexture::SubresourceMetadata::Offset() const noexcept {
		return ResourceMetadata_->PlacedSubresourceFootprints_[Index_Subresource_].Offset;
	}

	inline uint64_t ImageTexture::SubresourceMetadata::RowSizeInBytes() const noexcept {
		return ResourceMetadata_->RowSizesInBytes_[Index_Subresource_];
	}

	inline uint32_t ImageTexture::SubresourceMetadata::Num_Rows() const noexcept {
		return ResourceMetadata_->Nums_Rows_[Index_Subresource_];
	}

	//----	------	------	------	------	----//

	ImageTexture::SubresourceMetadata::SubresourceMetadata(const ResourceMetadata* resMetadata_, uint32_t idx_Subres_) :
		ResourceMetadata_{ resMetadata_ },
		Index_Subresource_{ idx_Subres_ } {}

	ImageTexture::SubresourceMetadata::~SubresourceMetadata() noexcept {}

	//////	//////	//////	//////	//////	//////
	//	ImageTexture::ResourceMetadata			//
	//////	//////	//////	//////	//////	//////

	inline ImageTexture::SubresourceMetadata ImageTexture::ResourceMetadata::operator[](uint32_t idx_Subres_) const noexcept {
		return SubresourceMetadata{ this, idx_Subres_ };
	}

	inline uint64_t ImageTexture::ResourceMetadata::SizeInBytes_IntermediateData() const noexcept {
		return SizeInBytes_IntermediateData_;
	}

	//----	------	------	------	------	----//

	ImageTexture::ResourceMetadata::ResourceMetadata(
		const GraphicsDevice& device_,
		const ImageTexture& tex_,
		const std::vector<D3D12_SUBRESOURCE_DATA>& subresources_
	) {
		const auto num_Subresources{ subresources_.size() };

		const size_t sizeInBytes_Memory{
			static_cast<size_t>(
				sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) +
				sizeof(uint64_t) +
				sizeof(uint32_t)
			) * num_Subresources
		};
		Logger().Message<0U>(
			"ImageTexture,{},Size required for texture metadata = {}\n",
			tex_.DebugName(),
			sizeInBytes_Memory
		);
		(sizeInBytes_Memory < static_cast<uint64_t>(-1)) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.ImageTexture - {}> The size required for allocating texture metadata is too large!\n",
				tex_.DebugName()
			)
		};

		ProcessHeap_ = { ::GetProcessHeap() };
		Memory_ = ::HeapAlloc(ProcessHeap_, 0, sizeInBytes_Memory);
		(Memory_ != nullptr) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.ImageTexture - {}> Failed to allocate memory for subresources data!\n",
				tex_.DebugName()
			)
		};

		PlacedSubresourceFootprints_ = static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(Memory_);
		RowSizesInBytes_ = reinterpret_cast<uint64_t*>(PlacedSubresourceFootprints_ + num_Subresources);
		Nums_Rows_ = reinterpret_cast<uint32_t*>(RowSizesInBytes_ + num_Subresources);

		device_->GetCopyableFootprints(
			&tex_.ResourceDesc_,
			0,
			static_cast<uint32_t>(num_Subresources),
			0,
			PlacedSubresourceFootprints_,
			Nums_Rows_,
			RowSizesInBytes_,
			&SizeInBytes_IntermediateData_
		);
		Logger().Message<0U>(
			"ImageTexture,{},Size required for intermediate data = {}\n",
			tex_.DebugName(),
			SizeInBytes_IntermediateData_
		);
		(SizeInBytes_IntermediateData_ < static_cast<uint64_t>(-1)) ||
		Utils::Debug::ThrowIfFalse{
			std::format(
				"<DX12.ImageTexture - {}> The size required to store the intermediate data is too large!\n",
				tex_.DebugName()
			)
		};
	}

	ImageTexture::ResourceMetadata::~ResourceMetadata() noexcept {
		if (Memory_ != nullptr) {
			::HeapFree(ProcessHeap_, 0, Memory_);
		}
	}

	//////	//////	//////	//////	//////	//////
	//	ImageTexture::Intermediate				//
	//////	//////	//////	//////	//////	//////

	inline ImageTexture::SubresourceMetadata ImageTexture::Intermediate::Metadata(uint32_t idx_Subres_) const noexcept {
		return (*Metadata_)[idx_Subres_];
	}
	inline const ImageTexture::ResourceMetadata& ImageTexture::Intermediate::Metadata() const noexcept {
		return *Metadata_;
	}
	inline const UploadBuffer& ImageTexture::Intermediate::Buffer() const noexcept {
		return Buffer_;
	}
	inline uint32_t ImageTexture::Intermediate::Num_Subresources() const noexcept {
		return static_cast<uint32_t>(Subresources_.size());
	}

	//----	------	------	------	------	----//

	void ImageTexture::Intermediate::GenerateSubresourceData(
		const GraphicsDevice& device_,
		const MipChain& mipChain_
	) {
		DirectX::PrepareUpload(
			device_.Get(),
			mipChain_(),
			mipChain_.Count(),
			mipChain_.Metadata(),
			Subresources_
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.ImageTexture - {}> Failed to generate subresource data using a mip chain!\n",
				ImageTexture_->DebugName()
			)
		};
	}

	void ImageTexture::Intermediate::AnalyzeSubresourceData(const GraphicsDevice& device_) {
		Metadata_ = UniPtr<ResourceMetadata>{
			new ResourceMetadata{
				device_,
				*ImageTexture_,
				Subresources_
			}
		};
	}

	// Copies subresource data into the intermediate buffer.
	// See UpdateSubresources in d3dx12.h for more details.
	void ImageTexture::Intermediate::CopySubresourceData(const GraphicsDevice& device_) {
		Buffer_.Initialize(
			device_,
			Metadata_->SizeInBytes_IntermediateData(),
			"IntermediateBuffer"
		);

		for (uint32_t i{ 0U }; i < Num_Subresources(); ++i) {
			const auto subresMetadata{ (*Metadata_)[i] };

			if (subresMetadata.RowSizeInBytes() > static_cast<uint64_t>(-1)) { throw std::runtime_error{ "" }; }

			D3D12_MEMCPY_DEST dstDesc{
				.pData { Buffer_() + subresMetadata.Offset() },
				.RowPitch{ subresMetadata.Footprint().RowPitch },
				.SlicePitch{
					static_cast<uint64_t>(subresMetadata.Footprint().RowPitch) *
					static_cast<uint64_t>(subresMetadata.Num_Rows()),
				},
			};
			::MemcpySubresource(
				&dstDesc,
				&(Subresources_.data()[i]),
				static_cast<uint64_t>(subresMetadata.RowSizeInBytes()),
				subresMetadata.Num_Rows(),
				subresMetadata.Footprint().Depth
			);
		}
	}

	//----	------	------	------	------	----//

	ImageTexture::Intermediate::Intermediate(
		const GraphicsDevice& device_,
		const ImageTexture& tex_,
		const MipChain& mipChain_
	) : ImageTexture_{ &tex_ } {
		GenerateSubresourceData(device_, mipChain_);
		AnalyzeSubresourceData(device_);
		CopySubresourceData(device_);
	}

	ImageTexture::Intermediate::~Intermediate() noexcept {
		Subresources_.clear();
	}
}

//****	******	******	******	******	****//

//////	//////	//////	//////	//////	//////
//	ImageTexture							//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	inline uint32_t ImageTexture::Width() const noexcept { return static_cast<uint32_t>(ResourceDesc_.Width); }
	inline uint32_t ImageTexture::Height() const noexcept { return static_cast<uint32_t>(ResourceDesc_.Height); }

	inline ImageTexture::STATUS ImageTexture::Status() const noexcept { return Status_; }
	inline const ImageTexture::Intermediate& ImageTexture::IntermediateData() const noexcept { return *IntermediateData_; }

	inline D3D12_SHADER_RESOURCE_VIEW_DESC ImageTexture::SRVDesc() const noexcept {
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
			.Format{ ResourceDesc_.Format },
			.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			.Texture2D{ .MipLevels{ static_cast<uint32_t>(ResourceDesc_.MipLevels) }, },
		};
		return srvDesc;
	}

	//----	------	------	------	------	----//

	inline void ImageTexture::CreateIntermediateData(
		const GraphicsDevice& device_,
		const MipChain& mipChain_
	) {
		if (IntermediateData_ == nullptr) {
			IntermediateData_ = new Intermediate{ device_, *this, mipChain_ };
		}
	}

	inline void ImageTexture::ReleaseIntermediateData() noexcept {
		if (IntermediateData_ != nullptr) {
			delete IntermediateData_;
			IntermediateData_ = nullptr;
		}
	}

	//----	------	------	------	------	----//

	void ImageTexture::Initialize(
		const GraphicsDevice& device_,
		const MipChain& mipChain_,
		std::string_view debugName_
	) {
		auto const& mipChainMetadata{ mipChain_.Metadata() };

		DefaultTexture2D::Initialize(
			device_,
			static_cast<uint32_t>(mipChainMetadata.width),
			static_cast<uint32_t>(mipChainMetadata.height),
			static_cast<uint16_t>(mipChainMetadata.mipLevels),
			mipChainMetadata.format,
			debugName_
		);

		// Reobtains the resource description.
		ResourceDesc_ = Wrapped_->GetDesc();

		CreateIntermediateData(device_, mipChain_);
		Logger().Message<0U>(
			"ImageTexture,{},Intermediate data created sucessfully.\n",
			debugName_
		);
	}

	//----	------	------	------	------	----//

	UniPtr<ImageTexture> ImageTexture::Create(
		GraphicsDevice const& device_,
		MipChain const& mipChain_,
		std::string_view debugName_
	) {
		auto imgTex{ UniPtr<ImageTexture>{ new ImageTexture{} } };
		imgTex->Initialize(device_, mipChain_, debugName_);
		return imgTex;
	}

	//----	------	------	------	------	----//

	constexpr ImageTexture::ImageTexture() noexcept {}

	ImageTexture::~ImageTexture() noexcept {
		ReleaseIntermediateData();
	}
}

//////	//////	//////	//////	//////	//////
//	ImageTextureUploader					//
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {
	inline ImageTextureUploader& ImageTextureUploader::operator<<(ImageTexture& tex_) {
		Batch(tex_);
		return *this;
	}

	//----	------	------	------	------	----//

	bool ImageTextureUploader::Begin() {
		(IsInBeginEndBlock_ == 0) ||
		Utils::Debug::ThrowIfFalse<std::logic_error>{
			"<DX12.ImageTextureUploader> Can't call Begin() inside a Begin-End block!\n"
		};

		if (BatchedTextures_.empty()) {
			IsInBeginEndBlock_ = 1;
			return true;
		}
		return false;
	}

	std::future<void> ImageTextureUploader::End(CommandQueue& cmdQueue_) {
		(IsInBeginEndBlock_ == 1) ||
		Utils::Debug::ThrowIfFalse<std::logic_error>{
			"<DX12.ImageTextureUploader> Can't call End() outside a Begin-End block!\n"
		};

		IsInBeginEndBlock_ = 0;

		auto upload{
			[&, this]() {
				// Orders the intermediate data of the batched textures
				// to be copied to the resources in the default heap.
				for (auto const* tex : BatchedTextures_) {
					auto const& data{ tex->IntermediateData() };
					// Code extracted from UpdateResource
					for (uint32_t i{ 0U }; i < data.Num_Subresources(); ++i) {
						D3D12_TEXTURE_COPY_LOCATION const dst{
							.pResource{ tex->Get() },
							.Type{ D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX },
							.SubresourceIndex{ i },
						};
						D3D12_TEXTURE_COPY_LOCATION const src{
							.pResource{ data.Buffer().Get() },
							.Type{ D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT },
							.PlacedFootprint{ data.Metadata(i).PlacedFootprint()},
						};
						CommandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
					}
				}
				// Orders transitions of all batched textures at once in one call.
				CommandList_->ResourceBarrier(
					static_cast<uint32_t>(BatchedResourceBarriers_.size()),
					BatchedResourceBarriers_.data()
				);

				// Puts the command list into the batch of the command queue.
				cmdQueue_ << CommandList_;
				// Submits the jobs.
				uint64_t fenceValue{ cmdQueue_.ExecuteBatchedCommandLists() };
				// Waits on the GPU-complete notification.
				DWORD const result{ cmdQueue_.CPUWait(fenceValue) };
				if (result != WAIT_OBJECT_0) {
					if (result == WAIT_FAILED) {
						throw std::system_error{
							std::error_code{ static_cast<int>(GetLastError()), std::system_category() },
							"<ImageTextureUploader> Error : WaitForSingleObject"
						};
					}
					else {
						throw std::runtime_error{ "<ImageTextureUploader> Error : WaitForSingleObject" };
					}
				}
				CommandList_.Reset(CommandAllocator_);

				for (auto* tex : BatchedTextures_) { tex->Status_ = ImageTexture::STATUS::READY_TO_USE; }

				// Releases intermediate data of the batched textures after the copy commands are finished.
				if (Thread_ClearBatch_.joinable()) { Thread_ClearBatch_.join(); }
				Thread_ClearBatch_ = std::thread{
					[this]() {
						ClearBatch();
					}
				};
			}
		};

		// Kicks off a thread that waits for the upload to complete on the GPU timeline.
		return std::future<void>{
			std::async(
				std::launch::async,
				upload
			)
		};
	}

	void ImageTextureUploader::Batch(ImageTexture& tex_) {
		(IsInBeginEndBlock_ == 1) ||
		Utils::Debug::ThrowIfFalse<std::logic_error>{
			"<DX12.ImageTextureUploader> Can't batch ready-to-upload textures outside a Begin-End block!\n"
		};

		if (tex_.Status_ == ImageTexture::STATUS::READY_TO_UPLOAD) {
			tex_.Status_ = ImageTexture::STATUS::UPLOAD_IN_PROGRESS;

			BatchedTextures_.emplace_back(&tex_);
			BatchedResourceBarriers_.emplace_back(
				D3D12_RESOURCE_BARRIER{
					.Type{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION },
					.Flags{ D3D12_RESOURCE_BARRIER_FLAG_NONE },
					.Transition{
						.pResource{ tex_.Get() },
						.Subresource{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						.StateBefore{ D3D12_RESOURCE_STATE_COPY_DEST },
						.StateAfter{ D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
					},
				}
			);
		}
	}

	//----	------	------	------	------	----//

	void ImageTextureUploader::ClearBatch() noexcept {
		auto& logger{ Utils::Debug::Logger::Default() };
		auto tid{ std::this_thread::get_id() };

		logger.Message<0U>(
			"<ImageTextureUploader @Thread {}> Starting clear the batched data...\n",
			*reinterpret_cast<uint32_t*>(&tid)
		);

		for (auto* tex : BatchedTextures_) {
			tex->ReleaseIntermediateData();
		}
		BatchedTextures_.clear();
		BatchedResourceBarriers_.clear();

		logger.Message<0U>(
			"<ImageTextureUploader @Thread {}> Batched data cleared completely.\n",
			*reinterpret_cast<uint32_t*>(&tid)
		);
	}

	//----	------	------	------	------	----//

	void ImageTextureUploader::Initialize(GraphicsDevice const& device_) {
		BatchedTextures_.clear();
		BatchedResourceBarriers_.clear();

		CommandAllocator_.Initialize(
			device_,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			"CmdAllocator@TexUploader"
		);
		CommandList_.Initialize(
			device_,
			CommandAllocator_,
			"CmdList@TexUploader"
		);
	}

	//----	------	------	------	------	----//

	ImageTextureUploader::ImageTextureUploader() noexcept {}

	ImageTextureUploader::~ImageTextureUploader() noexcept {
		if (Thread_ClearBatch_.joinable()) {
			Thread_ClearBatch_.join();
		}
	}
}