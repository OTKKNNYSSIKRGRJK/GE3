module;

#include<wrl.h>

export module Lumina.DX12 : GraphicsDevice;

//****	******	******	******	******	****//

//----	------	------	------	------	----//
//	Standard C++ library imports			//
//----	------	------	------	------	----//

import <cstdint>;

import <vector>;

//----	------	------	------	------	----//
//	Platform API imports					//
//----	------	------	------	------	----//

import <d3d12.h>;
import <dxgi1_6.h>;

//----	------	------	------	------	----//
//	Lumina library imports					//
//----	------	------	------	------	----//

import : Wrapper;

import : Debug;

//----	------	------	------	------	----//

import Lumina.Mixins;

import Lumina.Utils.String;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//----	------	------	------	------	----//
	//	Declaration								//
	//----	------	------	------	------	----//

	export class GraphicsDevice final :
		public Wrapper<GraphicsDevice, ID3D12Device>,
		public NonCopyable<GraphicsDevice> {
	public:
		inline auto Factory() const noexcept;
		inline auto Adapter() const noexcept;

		//----	------	------	------	------	----//

	#if defined(_DEBUG)
	public:
		void SetBreakOnMessages(D3D12_MESSAGE_SEVERITY minSeverity_) const;
		void SuppressMessages(
			std::vector<D3D12_MESSAGE_ID> const& ids_,
			std::vector<D3D12_MESSAGE_SEVERITY> const& severities_ = {},
			std::vector<D3D12_MESSAGE_CATEGORY> const& categories_ = {}
		) const;
	#endif

		//----	------	------	------	------	----//
	
	private:
		void CreateDXGIFactory(std::string_view debugName_);
		void CreateDXGIAdapter(std::string_view debugName_);
		void CreateD3D12Device(std::string_view debugName_);

		//----	------	------	------	------	----//

	public:
		void Initialize(std::string_view debugName_ = "GraphicsDevice");

		//----	------	------	------	------	----//

	public:
		inline GraphicsDevice() noexcept;
		virtual ~GraphicsDevice() noexcept;

		//====	======	======	======	======	====//

	private:
		Microsoft::WRL::ComPtr<IDXGIFactory7> Factory_{ nullptr };
		Microsoft::WRL::ComPtr<IDXGIAdapter4> Adapter_{ nullptr };
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	inline auto GraphicsDevice::Factory() const noexcept { return Factory_; }
	inline auto GraphicsDevice::Adapter() const noexcept { return Adapter_; }

	//----	------	------	------	------	----//

	#if defined(_DEBUG)
	void GraphicsDevice::SetBreakOnMessages(D3D12_MESSAGE_SEVERITY minSeverity_) const {
		ID3D12InfoQueue* infoQueue{ nullptr };
		if (SUCCEEDED(Wrapped_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
			for (
				D3D12_MESSAGE_SEVERITY severity : {
					D3D12_MESSAGE_SEVERITY_CORRUPTION,
					D3D12_MESSAGE_SEVERITY_ERROR,
					D3D12_MESSAGE_SEVERITY_WARNING,
					D3D12_MESSAGE_SEVERITY_INFO,
					D3D12_MESSAGE_SEVERITY_MESSAGE,
				}
			) {
				infoQueue->SetBreakOnSeverity(
					severity,
					(static_cast<int32_t>(severity) <= static_cast<int32_t>(minSeverity_))
				);
			}

			infoQueue->Release();
		}
	}

	void GraphicsDevice::SuppressMessages(
		std::vector<D3D12_MESSAGE_ID> const& ids_,
		std::vector<D3D12_MESSAGE_SEVERITY> const& severities_,
		std::vector<D3D12_MESSAGE_CATEGORY> const& categories_
	) const {
		ID3D12InfoQueue* infoQueue{ nullptr };
		if (SUCCEEDED(Wrapped_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
			D3D12_INFO_QUEUE_FILTER msgFilter{
				.DenyList{
					.NumCategories{ static_cast<uint32_t>(categories_.size()) },
					.pCategoryList{ const_cast<D3D12_MESSAGE_CATEGORY*>(categories_.data()) },
					.NumSeverities{ static_cast<uint32_t>(severities_.size()) },
					.pSeverityList{ const_cast<D3D12_MESSAGE_SEVERITY*>(severities_.data()) },
					.NumIDs{ static_cast<uint32_t>(ids_.size()) },
					.pIDList{ const_cast<D3D12_MESSAGE_ID*>(ids_.data()) },
				},
			};
			infoQueue->PushStorageFilter(&msgFilter);

			infoQueue->Release();
		}
	}
	#endif

	//----	------	------	------	------	----//

	// Creates a DXGI factory, which is necessary for creating devices and swap chains.
	void GraphicsDevice::CreateDXGIFactory(std::string_view debugName_) {
		::CreateDXGIFactory(IID_PPV_ARGS(Factory_.GetAddressOf())) ||
		Utils::Debug::ThrowIfFailed{
			"<DX12.GraphicsDevice> Failed to create a DXGI factory!\n"
		};
		Logger().Message<0U>(
			"GraphicsDevice,{},DXGI factory created successfully.\n",
			debugName_
		);
	}

	// Enumerates available hardware adapters in order of better performance, and use one as good as possible.
	void GraphicsDevice::CreateDXGIAdapter(std::string_view debugName_) {
		for (
			uint32_t i{ 0U };
			Factory_->EnumAdapterByGpuPreference(
				i,
				DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(Adapter_.GetAddressOf())
			) != DXGI_ERROR_NOT_FOUND;
			++i
		) {
			DXGI_ADAPTER_DESC3 adapterDesc{};
			Adapter_->GetDesc3(&adapterDesc) ||
			Utils::Debug::ThrowIfFailed{
				"<DX12.GraphicsDevice> Failed to obtain an adapter description!\n"
			};
			Logger().Message<0U>(
				"GraphicsDevice,{},Hardware adapter description obtained successfully.\n",
				debugName_
			);

			// The adapter is desired if it's not a software.
			if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
				Logger().Message<0U>(
					"GraphicsDevice,{},Hardware adapter in use: {}\n",
					debugName_,
					Utils::String::Convert(adapterDesc.Description)
				);
				break;
			}
			// Ignores the result if it's a software adapter.
			Adapter_ = nullptr;
		}

		(Adapter_ != nullptr) ||
		Utils::Debug::ThrowIfFalse{
			"<DX12.GraphicsDevice> Failed to find an appropriate hardware adapter!\n"
		};
		Logger().Message<0U>(
			"GraphicsDevice,{},Hardware adapter obtained successfully.\n",
			debugName_
		);
	}

	// Creates a device with feature level as high as possible.
	void GraphicsDevice::CreateD3D12Device(std::string_view debugName_) {
		std::vector<std::pair<D3D_FEATURE_LEVEL, std::string>> featureLevels{
			{ D3D_FEATURE_LEVEL_12_2, "12.2" },
			{ D3D_FEATURE_LEVEL_12_1, "12.1" },
			{ D3D_FEATURE_LEVEL_12_0, "12.0" },
		};
		for (auto const& featureLevel : featureLevels) {
			HRESULT hr_CreateDevice{
				::D3D12CreateDevice(
					Adapter_.Get(),
					featureLevel.first,
					IID_PPV_ARGS(Wrapped_.GetAddressOf())
				)
			};
			if (SUCCEEDED(hr_CreateDevice)) {
				Logger().Message<0U>(
					"GraphicsDevice,{},Feature level: {}\n",
					debugName_,
					featureLevel.second
				);
				break;
			}
		}

		(Wrapped_ != nullptr) ||
		Utils::Debug::ThrowIfFalse{
			"<DX12.GraphicsDevice> Failed to create the device!\n"
		};
		Logger().Message<0U>(
			"GraphicsDevice,{},Device created successfully.\n",
			debugName_
		);
	}

	//----	------	------	------	------	----//

	void GraphicsDevice::Initialize(std::string_view debugName_) {
		ThrowIfInitialized(debugName_);

		CreateDXGIFactory(debugName_);
		CreateDXGIAdapter(debugName_);
		CreateD3D12Device(debugName_);

		SetDebugName(debugName_);

		#if defined(_DEBUG)
		// Makes the application break upon occurance of
		// D3D12 messages of CORRUPTION, ERROR and WARNING severities.
		SetBreakOnMessages(D3D12_MESSAGE_SEVERITY_WARNING);
		SuppressMessages(
			{
				// Bug occured by interactions between the DXGI debug layer and DX12 debug layer on Windows 11; fixed already probably?
				// Reference: https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			},
			{ D3D12_MESSAGE_SEVERITY_INFO, }
		);
		#endif
	}

	//----	------	------	------	------	----//

	inline GraphicsDevice::GraphicsDevice() noexcept {}

	GraphicsDevice::~GraphicsDevice() noexcept {
		//auto& logger{ Logger() };

		//Adapter_->Release();
		//logger.Message<0U>("GraphicsDevice,{},Hardware adapter released successfully.\n", DebugName());

		//Factory_->Release();
		//logger.Message<0U>("GraphicsDevice,{},DXGI factory released successfully.\n", DebugName());
	}
}