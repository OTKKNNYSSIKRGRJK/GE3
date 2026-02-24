module;

#include<cstdint>

#include<vector>

#include<string>
#include<format>

#include<d3d12.h>

#include<wrl.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : RootSignature;

//****	******	******	******	******	****//

import : GraphicsDevice;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

export namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	RootSignature							//
	//////	//////	//////	//////	//////	//////

	class RootSignature final :
		public Wrapper<RootSignature, ID3D12RootSignature>,
		private NonCopyable<RootSignature> {
	public:
		class Setup;

		//====	======	======	======	======	====//

	public:
		void Initialize(
			GraphicsDevice const& device_,
			Setup const& rsSetup_,
			std::string_view debugName_ = "RootSignature"
		);

		//----	------	------	------	------	----//

	public:
		constexpr RootSignature() noexcept = default;
		constexpr virtual ~RootSignature() noexcept = default;
	};

	//////	//////	//////	//////	//////	//////
	//	RootSignature::Setup					//
	//////	//////	//////	//////	//////	//////

	class RootSignature::Setup final {
	public:
		// Creates a root signature description based on the setup.
		constexpr D3D12_ROOT_SIGNATURE_DESC operator()(
			D3D12_ROOT_SIGNATURE_FLAGS flags_ =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
				D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
		) const noexcept;
		// Does something much similar to what the overloaded operator() does, but in a stream-like manner.
		friend constexpr D3D12_ROOT_SIGNATURE_DESC& operator<<(
			D3D12_ROOT_SIGNATURE_DESC& rsDesc_,
			Setup const& rsSetup_
		);

		//----	------	------	------	------	----//

	public:
		constexpr Setup() noexcept = default;
		constexpr ~Setup() noexcept = default;

		//====	======	======	======	======	====//

	public:
		std::vector<D3D12_ROOT_PARAMETER> Parameters{};
		std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> DescriptorTables{};
		std::vector<D3D12_STATIC_SAMPLER_DESC> StaticSamplers{};
	};
}

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	RootSignature							//
	//////	//////	//////	//////	//////	//////

	void RootSignature::Initialize(
		GraphicsDevice const& device_,
		RootSignature::Setup const& rsSetup_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		D3D12_ROOT_SIGNATURE_DESC const rsDesc{ rsSetup_() };

		ID3DBlob* rsBlob{ nullptr };
		ID3DBlob* errorBlob{ nullptr };

		constexpr auto msg_Failure{
			[](HRESULT hr_SerializeRS_, ID3DBlob* errorBlob_) -> std::string {
				if (SUCCEEDED(hr_SerializeRS_)) { return ""; }

				std::string msg{ reinterpret_cast<char*>(errorBlob_->GetBufferPointer()) };
				errorBlob_->Release();

				return msg;
			}
		};

		HRESULT hr_SerializeRS{
			::D3D12SerializeRootSignature(
				&rsDesc,
				D3D_ROOT_SIGNATURE_VERSION_1,
				&rsBlob,
				&errorBlob
			)
		};
		hr_SerializeRS || Utils::Debug::ThrowIfFailed{ msg_Failure(hr_SerializeRS, errorBlob) };
		Logger().Message<0U>(
			"RootSignature,{},Root signature serializad successfully.\n",
			debugName_
		);

		device_->CreateRootSignature(
			0U,
			rsBlob->GetBufferPointer(),
			rsBlob->GetBufferSize(),
			IID_PPV_ARGS(&Wrapped_)
		) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.RootSignature> Failed to create {}!\n", debugName_
			)
		};
		Logger().Message<0U>(
			"RootSignature,{},Root signature created successfully.\n",
			debugName_
		);

		rsBlob->Release();

		SetDebugName(debugName_);
	}

	//////	//////	//////	//////	//////	//////
	//	RootSignature::Desc						//
	//////	//////	//////	//////	//////	//////

	constexpr D3D12_ROOT_SIGNATURE_DESC RootSignature::Setup::operator()(
		D3D12_ROOT_SIGNATURE_FLAGS flags_
	) const noexcept {
		return D3D12_ROOT_SIGNATURE_DESC{
			.NumParameters{ static_cast<uint32_t>(Parameters.size()) },
			.pParameters{ Parameters.data() },
			.NumStaticSamplers{ static_cast<uint32_t>(StaticSamplers.size()) },
			.pStaticSamplers{ StaticSamplers.data() },
			.Flags{ flags_ },
		};
	}

	constexpr D3D12_ROOT_SIGNATURE_DESC& operator<<(
		D3D12_ROOT_SIGNATURE_DESC& rsDesc_,
		RootSignature::Setup const& rsSetup_
	) {
		rsDesc_ = D3D12_ROOT_SIGNATURE_DESC{
			.NumParameters{ static_cast<uint32_t>(rsSetup_.Parameters.size()) },
			.pParameters{ rsSetup_.Parameters.data() },
			.NumStaticSamplers{ static_cast<uint32_t>(rsSetup_.StaticSamplers.size()) },
			.pStaticSamplers{ rsSetup_.StaticSamplers.data() },
			.Flags{ D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT },
		};

		return rsDesc_;
	}
}