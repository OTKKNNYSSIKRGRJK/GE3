module;

#include<d3d12.h>

export module Lumina.AssetManager;

import <memory>;

import <vector>;
import <unordered_map>;

import <string>;

import : Graphics;
import : Audio;

import Lumina.Utils.String;
import Lumina.Utils.Debug;

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

namespace Lumina {
	class AssetManager;

	class GraphicsContext;
	class AudioContext;
}

namespace Lumina {
	class GraphicsContext final {
		friend AssetManager;

	public:
		constexpr auto CPUHandle(uint32_t viewID_) const noexcept {
			return DX12DescriptorManager_.CPUHandle(viewID_);
		}

	public:
		// Creates and uploads textures based on image data loaded from files.
		// Generates texture IDs which are to be used 
		void LoadImageTextures(
			std::vector<uint32_t>& texIDs_,
			std::vector<std::pair<std::string, std::string>> const arr_Pairs_TexName_ImgFilePath_
		) {
			std::vector<DX12::ResourceID> texResIDs{};
			texResIDs.reserve(arr_Pairs_TexName_ImgFilePath_.size());

			// Creates textures but does not perform the copies at once.
			for (auto const& pair : arr_Pairs_TexName_ImgFilePath_) {
				std::string const& texName{ pair.first };
				std::string const& texFilePath{ pair.second };
				texResIDs.emplace_back(
					DX12ResourceManager_.CreateImageTexture(
						texName,
						texFilePath
					)
				);
			}

			// Uploads the resources in one go.
			DX12ResourceManager_.UploadResources(texResIDs);

			//----	------	------	------	------	----//

			texIDs_.clear();
			texIDs_.reserve(arr_Pairs_TexName_ImgFilePath_.size());

			// Creates non-shader-visible SRVs.
			// Obtains texture IDs which serve as the keys to the SRVs.
			for (DX12::ResourceID const texResID : texResIDs) {
				texIDs_.emplace_back(
					DX12DescriptorManager_.CreateSRV<void>(
						DX12ResourceManager_,
						texResID
					)
				);
			}
		}

		template<typename ResourceType>
		DX12::ViewID CreateCBV(ResourceType const& res_) { return DX12DescriptorManager_.CreateCBV(res_); }
		template<typename ResourceType>
		DX12::ViewID CreateSRV(ResourceType const& res_) { return DX12DescriptorManager_.CreateSRV(res_); }
		template<typename ResourceType>
		DX12::ViewID CreateUAV(ResourceType const& res_) { return DX12DescriptorManager_.CreateUAV(res_); }

	private:
		void Initialize(DX12::Context const& dx12Context_) {
			DX12ResourceManager_.Initialize(dx12Context_);
			DX12DescriptorManager_.Initialize(dx12Context_);
		}

		void Finalize() noexcept {}

	private:
		DX12::ResourceManager DX12ResourceManager_{};
		DX12::DescriptorManager DX12DescriptorManager_{};
	};

	class AudioContext {
		friend AssetManager;

	public:
		auto LoadFromFile(std::string_view filePath_)
			-> XAudio2::AudioStream::Handle {
			return Manager_.LoadFromFile(Utils::String::Convert(filePath_));
		}

		auto Play(
			XAudio2::AudioStream::Handle hndl_Stream_,
			bool flag_Loop_,
			float volume_
		) -> XAudio2::AudioStreamPlayer::Handle {
			return Manager_.Play(hndl_Stream_, flag_Loop_, volume_);
		}

	private:
		void Initialize() {
			Manager_.Initialize();
		}

		void Finalize() noexcept {
			Manager_.Finalize();
		}

	private:
		XAudio2::AudioManager Manager_{};
	};
}

namespace Lumina {
	export class AssetManager {
	public:
		GraphicsContext& Graphics() { return (*Graphics_); }
		AudioContext& Audio() { return (*Audio_); }

	public:
		void Initialize(DX12::Context const& dx12Context_);
		void Finalize() noexcept;

	private:
		UniPtr<GraphicsContext> Graphics_;
		UniPtr<AudioContext> Audio_;
	};

	void AssetManager::Initialize(DX12::Context const& dx12Context_) {
		Graphics_ = UniPtr<GraphicsContext>{ new GraphicsContext{} };
		Graphics_->Initialize(dx12Context_);

		Audio_ = UniPtr<AudioContext>{ new AudioContext{} };
		Audio_->Initialize();
	}

	void AssetManager::Finalize() noexcept {
		Graphics_->Finalize();
		Audio_->Finalize();
	}
}