module;

#include<d3d12.h>

export module Lumina.AssetManager : Graphics;

import <memory>;

import <vector>;
import <unordered_map>;

import <string>;

import Lumina.DX12;
import Lumina.DX12.Context;
import Lumina.DX12.Aux.View;

import Lumina.Utils.Debug;

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

namespace Lumina::DX12 {
	class ResourceManager;
	class DescriptorManager;
}

namespace Lumina::DX12 {
	using ResourceID = uint32_t;
	using ViewID = uint32_t;

	enum class RESOURCE_TYPE {
		DEFAULT_BUFFER = 0x00U,
		UPLOAD_BUFFER = 0x01U,
		UNORDERED_ACCESS_BUFFER = 0x02U,
		READBACK_BUFFER = 0x03U,
		IMAGE_TEXTURE2D = 0x10U,
	};
	enum class VIEW_TYPE {
		CBV = 0U,
		SRV = 1U,
		UAV = 2U,
	};
	enum class SHADER_TYPE {
		VERTEX_SHADER = 0U,
		PIXEL_SHADER = 1U,
	};
}

namespace Lumina::DX12 {
	class ResourceManager final {
		friend DescriptorManager;

	private:
		static constexpr RESOURCE_TYPE GetResourceType(ResourceID resID_) noexcept {
			return static_cast<RESOURCE_TYPE>(resID_ >> BitOffset_Index_ResourceType_);
		}
		static constexpr uint32_t GetResourceIndex(ResourceID resID_) noexcept {
			return (resID_ & BitMask_ResourceIndex_);
		}

		auto GetResource(ResourceID resID_) const noexcept -> IResource* {
			RESOURCE_TYPE resType{ GetResourceType(resID_) };
			uint32_t resIdx{ GetResourceIndex(resID_) };

			switch (resType) {
				case RESOURCE_TYPE::IMAGE_TEXTURE2D:
					return reinterpret_cast<IResource*>(Arr_ImageTextures_.at(resIdx).get());
			}

			return nullptr;
		}

	public:
		// Creates a texture from the loaded image,
		// but the data is not yet copied to the VRAM.
		ResourceID CreateImageTexture(
			std::string_view name_,
			std::string_view filePath_
		);

		// Copies all the resource data to the VRAM.
		void UploadResources(
			std::vector<ResourceID> const& unuploadedResIDs_
		);

	public:
		void Initialize(Context const& context_);

	private:
		std::vector<UniPtr<DefaultBuffer>> Arr_DefaultBuffers_{};
		std::vector<UniPtr<UploadBuffer>> Arr_UploadBuffers_{};
		std::vector<UniPtr<UnorderedAccessBuffer>> Arr_UorderedAccessBuffers_{};
		std::vector<UniPtr<ReadbackBuffer>> Arr_ReadbackBuffers_{};

		std::vector<UniPtr<ImageTexture>> Arr_ImageTextures_{};
		std::unordered_map<std::string, ImageTexture const*> Dict_ImageTextures_{};

	private:
		ImageTextureUploader Uploader_{};

	private:
		Context const* Context_{ nullptr };

	private:

		static constexpr uint32_t BitOffset_Index_ResourceType_{ 20U };
		static constexpr uint32_t BitMask_ResourceIndex_{ (1U << BitOffset_Index_ResourceType_) - 1U };
	};

	ResourceID ResourceManager::CreateImageTexture(
		std::string_view name_,
		std::string_view filePath_
	) {
		(Dict_ImageTextures_.find(name_.data()) == Dict_ImageTextures_.cend()) ||
		Utils::Debug::ThrowIfFalse{
			"<ResourceManager> Texture of the same name is extant!\n"
		};

		auto img{ ImageSet::Create(filePath_) };
		auto mipChain{ MipChain::Create(*img) };
		auto tex{ ImageTexture::Create(Context_->Device(), *mipChain, name_) };

		uint32_t const idx_Res{ static_cast<uint32_t>(Arr_ImageTextures_.size()) };
		uint32_t const idx_ResType{ static_cast<uint32_t>(RESOURCE_TYPE::IMAGE_TEXTURE2D) };
		ResourceID const texID{ idx_Res | (idx_ResType << BitOffset_Index_ResourceType_) };

		Dict_ImageTextures_.emplace(name_, tex.get());
		Arr_ImageTextures_.emplace_back(std::move(tex));

		return texID;
	}

	void ResourceManager::UploadResources(
		std::vector<ResourceID> const& unuploadedResIDs_
	) {
		Uploader_.Begin();
		for (ResourceID resID : unuploadedResIDs_) {
			auto* res{ GetResource(resID) };
			RESOURCE_TYPE const resType{ GetResourceType(resID) };
			if (resType == RESOURCE_TYPE::IMAGE_TEXTURE2D) {
				Uploader_ << (*reinterpret_cast<ImageTexture*>(res));
			}
		}
		[[maybe_unused]] auto future_UploadTexs{ Uploader_.End(Context_->DirectQueue()) };
	}

	void ResourceManager::Initialize(Context const& context_) {
		Context_ = &context_;

		Uploader_.Initialize(Context_->Device());
	}
}

namespace Lumina::DX12 {
	class DescriptorManager final {
	private:
		constexpr void AllocateDescriptor(
			ViewID& viewID_,
			D3D12_CPU_DESCRIPTOR_HANDLE& viewCPUHandle_,
			VIEW_TYPE viewType_
		) {
			uint32_t const viewTypeIdx{ static_cast<uint32_t>(viewType_) };
			uint32_t const viewIdx{ Counts_View_[viewTypeIdx] };
			viewCPUHandle_.ptr = Tables_Local_[viewTypeIdx].CPUHandle(viewIdx).ptr;
			++Counts_View_[viewTypeIdx];
			viewID_ = viewIdx | (viewTypeIdx << BitOffset_ViewTypeIndex_);
		}

	private:
		constexpr uint32_t GetViewTypeIndex(ViewID viewID_) const noexcept {
			return (viewID_ >> BitOffset_ViewTypeIndex_);
		}
		constexpr uint32_t GetViewIndex(ViewID viewID_) const noexcept {
			return (viewID_ & BitMask_ViewIndex_);
		}

		constexpr auto LocalTable(VIEW_TYPE viewType_) noexcept
			-> DescriptorTable& {
			return Tables_Local_[static_cast<uint32_t>(viewType_)];
		}
		constexpr auto GlobalTable(SHADER_TYPE shaderType_, VIEW_TYPE viewType_) noexcept
			-> DescriptorTable& {
			return Tables_Global_[static_cast<uint32_t>(shaderType_)][static_cast<uint32_t>(viewType_)];
		}

	public:
		constexpr auto CPUHandle(ViewID viewID_) const noexcept {
			uint32_t const viewTypeIdx{ GetViewTypeIndex(viewID_) };
			uint32_t const viewIdx{ GetViewIndex(viewID_) };

			return Tables_Local_[viewTypeIdx].CPUHandle(viewIdx);
		}

	public:
		// Copies local views to the global shader-visible heap according to the root signature setup.
		/*void Copy(std::vector<ViewID> const& localViewIDs_) {
			auto const& device{ Context_->Device() };
			device->CopyDescriptorsSimple(...);
		}*/

		template<typename ResourceType>
		ViewID CreateCBV(ResourceType const& res_) {
			ViewID viewID{};
			D3D12_CPU_DESCRIPTOR_HANDLE viewCPUHandle{};
			AllocateDescriptor(viewID, viewCPUHandle, VIEW_TYPE::CBV);
			ConstantBufferView::Create(Context_->Device(), viewCPUHandle, res_);

			return viewID;
		}

		template<typename ElementType, typename ResourceType>
		ViewID CreateSRV(ResourceType const& res_) {
			ViewID viewID{};
			D3D12_CPU_DESCRIPTOR_HANDLE viewCPUHandle{};
			AllocateDescriptor(viewID, viewCPUHandle, VIEW_TYPE::SRV);
			ShaderResourceView<ElementType>::Create(Context_->Device(), viewCPUHandle, res_);

			return viewID;
		}

		template<typename ElementType, typename ResourceType>
		ViewID CreateUAV(ResourceType const& res_) {
			ViewID viewID{};
			D3D12_CPU_DESCRIPTOR_HANDLE viewCPUHandle{};
			AllocateDescriptor(viewID, viewCPUHandle, VIEW_TYPE::UAV);
			UnorderedAccessView<ElementType>::Create(Context_->Device(), viewCPUHandle, res_);

			return viewID;
		}

		template<typename ElementType>
		ViewID CreateSRV(
			ResourceManager const& resMngr_,
			ResourceID resID_
		) {
			auto const* res{ resMngr_.GetResource(resID_) };
			RESOURCE_TYPE const resType{ ResourceManager::GetResourceType(resID_) };
			if (resType == RESOURCE_TYPE::IMAGE_TEXTURE2D) {
				return CreateSRV<ElementType>(*reinterpret_cast<ImageTexture const*>(res));
			}

			return 0xFFFFFFFFU;
		}

	public:
		void Initialize(DX12::Context const& context_);

	private:
		DescriptorHeap Heap_Local_{};
		DescriptorTable Tables_Local_[3]{};
		DescriptorTable Tables_Global_[2][3]{};

		uint32_t Counts_View_[3]{ 0U, 0U, 0U, };

	private:
		Context const* Context_{ nullptr };

	private:
		static constexpr uint32_t Capacity_LocalCBV_{ 1024U };
		static constexpr uint32_t Capacity_LocalSRV_{ 1024U };
		static constexpr uint32_t Capacity_LocalUAV_{ 1024U };

		static constexpr uint32_t BitOffset_ViewTypeIndex_{ 20U };
		static constexpr uint32_t BitMask_ViewIndex_{ (1U << BitOffset_ViewTypeIndex_) - 1U };
	};


	void DescriptorManager::Initialize(Context const& context_) {
		Context_ = &context_;

		Heap_Local_.Initialize(
			Context_->Device(),
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			(Capacity_LocalCBV_ + Capacity_LocalSRV_ + Capacity_LocalUAV_),
			false,
			"ResourceManager LocalHeap"
		);
		Heap_Local_.Allocate(LocalTable(VIEW_TYPE::CBV), Capacity_LocalCBV_);
		Heap_Local_.Allocate(LocalTable(VIEW_TYPE::SRV), Capacity_LocalSRV_);
		Heap_Local_.Allocate(LocalTable(VIEW_TYPE::UAV), Capacity_LocalUAV_);

		Context_->GlobalDescriptorHeap().Allocate(
			GlobalTable(SHADER_TYPE::VERTEX_SHADER, VIEW_TYPE::CBV),
			16U
		);
		Context_->GlobalDescriptorHeap().Allocate(
			GlobalTable(SHADER_TYPE::VERTEX_SHADER, VIEW_TYPE::SRV),
			48U
		);
		Context_->GlobalDescriptorHeap().Allocate(
			GlobalTable(SHADER_TYPE::VERTEX_SHADER, VIEW_TYPE::UAV),
			16U
		);
		Context_->GlobalDescriptorHeap().Allocate(
			GlobalTable(SHADER_TYPE::PIXEL_SHADER, VIEW_TYPE::CBV),
			16U
		);
		Context_->GlobalDescriptorHeap().Allocate(
			GlobalTable(SHADER_TYPE::PIXEL_SHADER, VIEW_TYPE::SRV),
			48U
		);
		Context_->GlobalDescriptorHeap().Allocate(
			GlobalTable(SHADER_TYPE::PIXEL_SHADER, VIEW_TYPE::UAV),
			16U
		);
	}
}