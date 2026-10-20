module;

// DirectXTex（画像の読み込み）。DX12.Texture.ixx と同じく、グローバルモジュールフラグメントで include する
#include<d3d12.h>
#include<External/DirectXTex/DirectXTex.h>
#pragma comment(lib, "DirectXTex.lib")

module Lumina.SDF.Adapter;

// ここで import しているヘッダユニットは、いずれも SDF.Adapter.ixx / SDF.Tests.ixx で import 済み
import <algorithm>;
import <cmath>;
import <cstdint>;
import <memory>;
import <optional>;
import <span>;
import <string>;
import <string_view>;
import <vector>;

import Lumina.Utils.String;

// Windows のマクロ（DirectXTex 経由で入る min / max）が std::min / std::max とぶつかるので消す
#undef min
#undef max

namespace Lumina::SDF {
	auto LoadCoverage(std::string_view path_, ImageOptions const& options_, std::string& error_) -> std::optional<Grid<float>> {
		// sRGB の扱いは無視して、ファイルの値のまま読む（α は元々変換されない）
		DirectX::ScratchImage loaded{};
		std::wstring const wpath{ Utils::String::Convert(path_) };
		if (FAILED(DirectX::LoadFromWICFile(wpath.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, loaded))) {
			error_ = "画像を読み込めませんでした: " + std::string{ path_ };
			return std::nullopt;
		}

		// 1 枚目を R8G8B8A8_UNORM にそろえる
		DirectX::Image const* image{ loaded.GetImage(0U, 0U, 0U) };
		if (image == nullptr) {
			error_ = "画像が空です: " + std::string{ path_ };
			return std::nullopt;
		}
		DirectX::ScratchImage converted{};
		if (image->format != DXGI_FORMAT_R8G8B8A8_UNORM) {
			if (FAILED(DirectX::Convert(*image, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted))) {
				error_ = "画像を RGBA8 に変換できませんでした: " + std::string{ path_ };
				return std::nullopt;
			}
			image = converted.GetImage(0U, 0U, 0U);
		}
		if (image == nullptr || image->width == 0U || image->height == 0U) {
			error_ = "画像の大きさが 0 です: " + std::string{ path_ };
			return std::nullopt;
		}

		uint32_t const w{ static_cast<uint32_t>(image->width) };
		uint32_t const h{ static_cast<uint32_t>(image->height) };
		Grid<float> coverage{ w, h, 0.0f };
		for (uint32_t y{ 0U }; y < h; ++y) {
			uint8_t const* const row{ image->pixels + static_cast<std::size_t>(y) * image->rowPitch };
			for (uint32_t x{ 0U }; x < w; ++x) {
				uint8_t const* const px{ row + static_cast<std::size_t>(x) * 4U };
				float v{ 0.0f };
				switch (options_.Source) {
					case Channel::Alpha:
						v = static_cast<float>(px[3]) / 255.0f;
						break;
					case Channel::Red:
						v = static_cast<float>(px[0]) / 255.0f;
						break;
					case Channel::Luminance:
						v = (0.2126f * static_cast<float>(px[0]) + 0.7152f * static_cast<float>(px[1]) + 0.0722f * static_cast<float>(px[2])) / 255.0f;
						break;
				}
				coverage.At(x, y) = options_.Invert ? (1.0f - v) : v;
			}
		}
		return coverage;
	}

	auto BakeFromImage(
		std::string_view path_,
		ImageOptions const& image_,
		BakeOptions const& bake_,
		std::string& error_
	) -> std::optional<Baked> {
		std::optional<Grid<float>> coverage{ LoadCoverage(path_, image_, error_) };
		if (!coverage) { return std::nullopt; }

		uint32_t const srcW{ coverage->Width };
		uint32_t const srcH{ coverage->Height };
		uint32_t const outW{ (bake_.Width > 0U) ? bake_.Width : srcW };
		uint32_t const outH{
			(bake_.Height > 0U)
				? bake_.Height
				: std::max(1U, static_cast<uint32_t>(std::lround(static_cast<double>(outW) * srcH / srcW)))
		};

		Grid<float> distance{ SignedDistance(*coverage, image_.Threshold) };
		Baked baked{};
		if (outW != srcW || outH != srcH) {
			distance = Downsample(distance, outW, outH);
			baked.Coverage = Resample(*coverage, outW, outH);
		}
		else {
			baked.Coverage = std::move(*coverage);
		}

		baked.Range = (bake_.Range > 0.0f) ? bake_.Range : 1.0f;
		baked.Encoded = EncodeUNorm8(distance, baked.Range);
		baked.Distance = std::move(distance);
		baked.SourceWidth = srcW;
		baked.SourceHeight = srcH;
		return baked;
	}

	auto CreateTexture(
		DX12::Context const& dxContext_,
		TexelFormat format_,
		uint32_t width_,
		uint32_t height_,
		std::span<uint8_t const> texels_,
		std::string_view debugName_
	) -> std::unique_ptr<DX12::DefaultTexture2D> {
		uint32_t const bytesPerTexel{ (format_ == TexelFormat::R8G8) ? 2U : 1U };
		DXGI_FORMAT const format{ (format_ == TexelFormat::R8G8) ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8_UNORM };
		if (width_ == 0U || height_ == 0U) { return nullptr; }
		if (texels_.size() != static_cast<std::size_t>(width_) * height_ * bytesPerTexel) { return nullptr; }
		std::size_t const rowBytes{ static_cast<std::size_t>(width_) * bytesPerTexel };

		auto const& device{ dxContext_.Device() };
		auto texture{ std::make_unique<DX12::DefaultTexture2D>() };
		texture->Initialize(device, width_, height_, 1U, 1U, format, debugName_);	// 作った時点で COPY_DEST

		// 行のピッチは 256 バイト境界なので、フットプリントを聞いてから詰める
		D3D12_RESOURCE_DESC const desc{ (*texture)->GetDesc() };
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		UINT numRows{ 0U };
		UINT64 rowSize{ 0U };
		UINT64 totalSize{ 0U };
		device->GetCopyableFootprints(&desc, 0U, 1U, 0U, &footprint, &numRows, &rowSize, &totalSize);

		DX12::UploadBuffer staging{};
		staging.Initialize(device, totalSize, "SDF.Staging");
		for (uint32_t y{ 0U }; y < height_; ++y) {
			staging.Store(
				texels_.data() + static_cast<std::size_t>(y) * rowBytes,
				rowBytes,
				footprint.Offset + static_cast<UINT64>(y) * footprint.Footprint.RowPitch
			);
		}

		DX12::CommandAllocator allocator{};
		allocator.Initialize(device);
		DX12::CommandList cmdList{};
		cmdList.Initialize(device, allocator);

		D3D12_TEXTURE_COPY_LOCATION dst{};
		dst.pResource = texture->Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0U;
		D3D12_TEXTURE_COPY_LOCATION src{};
		src.pResource = staging.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = footprint;
		cmdList->CopyTextureRegion(&dst, 0U, 0U, 0U, &src, nullptr);

		D3D12_RESOURCE_BARRIER const barrier{
			DX12::Barrier::Transition(*texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(1U, &barrier);

		// ParticleSystem::Initialize と同じ、その場で実行して待つやり方（staging はこの関数を抜けるまで生きている）
		dxContext_.DirectQueue() << cmdList;
		dxContext_.DirectQueue().CPUWait(dxContext_.DirectQueue().ExecuteBatchedCommandLists());
		return texture;
	}

	auto CreateTextureR8(
		DX12::Context const& dxContext_,
		uint32_t width_,
		uint32_t height_,
		std::span<uint8_t const> texels_,
		std::string_view debugName_
	) -> std::unique_ptr<DX12::DefaultTexture2D> {
		return CreateTexture(dxContext_, TexelFormat::R8, width_, height_, texels_, debugName_);
	}
}
