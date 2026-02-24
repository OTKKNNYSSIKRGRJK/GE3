module;

#include<d3d12.h>
#include<dxgi1_6.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : GraphicsDevice;

//****	******	******	******	******	****//

import <cstdint>;

import <utility>;

import <vector>;

import : Wrapper;

import : Debug;

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
		constexpr auto Factory() const noexcept;
		constexpr auto Adapter() const noexcept;

		//----	------	------	------	------	----//
	
	private:
		void CreateDXGIFactory(std::string_view debugName_);
		void CreateDXGIAdapter(std::string_view debugName_);
		void CreateD3D12Device(std::string_view debugName_);

		#if defined(_DEBUG)
		void EnableBreakOnAlert();
		#endif

		//----	------	------	------	------	----//

	public:
		void Initialize(std::string_view debugName_ = "GraphicsDevice");

		//----	------	------	------	------	----//

	public:
		constexpr GraphicsDevice() noexcept;
		virtual ~GraphicsDevice() noexcept;

		//====	======	======	======	======	====//

	private:
		IDXGIFactory7* Factory_{ nullptr };
		IDXGIAdapter4* Adapter_{ nullptr };
	};

	//----	------	------	------	------	----//
	//	Implementation							//
	//----	------	------	------	------	----//

	constexpr auto GraphicsDevice::Factory() const noexcept { return Factory_; }
	constexpr auto GraphicsDevice::Adapter() const noexcept { return Adapter_; }

	//----	------	------	------	------	----//

	// Creates a DXGI factory, which is necessary for creating devices and swap chains.
	void GraphicsDevice::CreateDXGIFactory(std::string_view debugName_) {
		::CreateDXGIFactory(IID_PPV_ARGS(&Factory_)) ||
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
				IID_PPV_ARGS(&Adapter_)
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
					Adapter_,
					featureLevel.first,
					IID_PPV_ARGS(&Wrapped_)
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

	#if defined(_DEBUG)
	void GraphicsDevice::EnableBreakOnAlert() {
		ID3D12InfoQueue* infoQueue{ nullptr };
		if (SUCCEEDED(Wrapped_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
			// Makes the application break upon occurance of severe errors.
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			// Makes the application break upon occurance of errors.
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			// Makes the application break upon occurance of warnings.
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

			D3D12_MESSAGE_ID denyIDs[]{
				// Bug occured by interactions between the DXGI debug layer and DX12 debug layer on Windows 11; fixed already probably?
				// Reference: https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			};
			D3D12_MESSAGE_SEVERITY severities[]{ D3D12_MESSAGE_SEVERITY_INFO, };
			D3D12_INFO_QUEUE_FILTER msgFilter{
				.DenyList{
					.NumSeverities{ _countof(severities) },
					.pSeverityList{ severities },
					.NumIDs{ _countof(denyIDs) },
					.pIDList{ denyIDs },
				},
			};
			// Filters out the message(s).
			infoQueue->PushStorageFilter(&msgFilter);

			infoQueue->Release();
		}
	}
	#endif

	//----	------	------	------	------	----//

	void GraphicsDevice::Initialize(std::string_view debugName_) {
		ThrowIfInitialized(debugName_);

		CreateDXGIFactory(debugName_);
		CreateDXGIAdapter(debugName_);
		CreateD3D12Device(debugName_);

		#if defined(_DEBUG)
		EnableBreakOnAlert();
		#endif

		SetDebugName(debugName_);
	}

	//----	------	------	------	------	----//

	constexpr GraphicsDevice::GraphicsDevice() noexcept {}

	GraphicsDevice::~GraphicsDevice() noexcept {
		auto& logger{ Logger() };

		Adapter_->Release();
		logger.Message<0U>("GraphicsDevice,{},Hardware adapter released successfully.\n", DebugName());

		Factory_->Release();
		logger.Message<0U>("GraphicsDevice,{},DXGI factory released successfully.\n", DebugName());
	}
}