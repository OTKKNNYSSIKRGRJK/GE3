module;

#include<d3d12.h>

// dxcapi.h				| DirectX shader compiler API
// dxcompiler.lib		| For HLSL compilation
#include<dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

#include<wrl.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : Shader;

//****	******	******	******	******	****//

import <cstdint>;

import <memory>;

import <vector>;

import <string>;
import <format>;

import : Wrapper;

import : Debug;

import Lumina.Mixins;

import Lumina.Utils.String;
import Lumina.Utils.Debug;

//////	//////	//////	//////	//////	//////

namespace {
	template<typename T>
	using COMPtr = Microsoft::WRL::ComPtr<T>;
}

//****	******	******	******	******	****//

export namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	Shader									//
	//////	//////	//////	//////	//////	//////

	class Shader final :
		public Wrapper<Shader, IDxcBlob>,
		public NonCopyable<Shader> {
	public:
		class Compiler;

		//====	======	======	======	======	====//

	public:
		constexpr std::wstring_view FilePath() const noexcept { return FilePath_; }
		constexpr std::wstring_view Profile() const noexcept { return Profile_; }

		//----	------	------	------	------	----//

	public:
		void Initialize(
			Compiler const& compiler_,
			std::wstring_view filePath_,
			std::wstring_view profile_,
			std::wstring_view entryPoint_,
			std::string_view debugName_ = "Shader"
		);

	public:
		constexpr Shader() noexcept = default;
		constexpr virtual ~Shader() noexcept = default;

		//----	------	------	------	------	----//

	private:
		std::wstring FilePath_{};
		std::wstring Profile_{};
	};

	//////	//////	//////	//////	//////	//////
	//	Shader::Compiler						//
	//////	//////	//////	//////	//////	//////

	class Shader::Compiler final :
		public Wrapper<Shader::Compiler, IDxcCompiler3>,
		private NonCopyable<Shader::Compiler> {
		friend Shader;

		//====	======	======	======	======	====//

	private:
		COMPtr<IDxcBlobEncoding> LoadFromFile(std::wstring_view filePath_) const;
		void Compile(
			Shader& shader_,
			COMPtr<IDxcBlobEncoding> const& shaderSource_,
			std::wstring_view entryPoint_
		) const;

		//----	------	------	------	------	----//

	public:
		void Initialize(std::string_view debugName_ = "ShaderCompiler");

		//----	------	------	------	------	----//

	public:
		constexpr Compiler() noexcept;
		virtual ~Compiler() noexcept;

		//====	======	======	======	======	====//

	private:
		IDxcUtils* Utils_{ nullptr };
		IDxcIncludeHandler* IncludeHandler_{ nullptr };
	};
}

//****	******	******	******	******	****//

namespace Lumina::DX12 {

	//////	//////	//////	//////	//////	//////
	//	Shader									//
	//////	//////	//////	//////	//////	//////

	void Shader::Initialize(
		Compiler const& compiler_,
		std::wstring_view filePath_,
		std::wstring_view profile_,
		std::wstring_view entryPoint_,
		std::string_view debugName_
	) {
		ThrowIfInitialized(debugName_);

		FilePath_ = filePath_;
		Profile_ = profile_;
		Logger().Message<0U>(
			"Shader,{},Compilation begins:\n,,Path = \"{}\"\n,,Profile = \"{}\"\n,,Entry point =\n",
			debugName_,
			Utils::String::Convert(FilePath_), Utils::String::Convert(Profile_)
		);

		COMPtr<IDxcBlobEncoding> shaderSource{ compiler_.LoadFromFile(FilePath_) };
		compiler_.Compile(*this, shaderSource, entryPoint_);
		Logger().Message<0U>(
			"Shader,{},Compilation ended successfully.\n",
			debugName_
		);

		SetDebugName(debugName_);
	}

	//////	//////	//////	//////	//////	//////
	//	Shader::Compiler						//
	//////	//////	//////	//////	//////	//////

	COMPtr<IDxcBlobEncoding> Shader::Compiler::LoadFromFile(std::wstring_view filePath_) const {
		COMPtr<IDxcBlobEncoding> shaderSource{ nullptr };
		Utils_->LoadFile(filePath_.data(), nullptr, &shaderSource) ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.Shader.Compiler> Failed to load \"{}\"!\n",
				Utils::String::Convert(filePath_)
			)
		};
		return shaderSource;
	}

	void Shader::Compiler::Compile(
		Shader& shader_,
		COMPtr<IDxcBlobEncoding> const& shaderSource_,
		std::wstring_view entryPoint_
	) const {
		DxcBuffer shaderSourceBuffer{
			.Ptr{ shaderSource_->GetBufferPointer() },
			.Size{ shaderSource_->GetBufferSize() },
			.Encoding{ DXC_CP_UTF8 },
		};
		std::vector<LPCWSTR> args{
			shader_.FilePath_.data(),
			L"-T", shader_.Profile_.data(),		// Specifies the Shader profile
			L"-E", entryPoint_.data(),			// Specifies the entry point
			L"-Zi", L"-Qembed_debug",			// Embeds debug information
			L"-Od",								// Deactives optimization
			L"-Zpr",							// Row-major
		};

		COMPtr<IDxcResult> shaderResult{ nullptr };

		constexpr auto msg_Failure{
			[](
				HRESULT hr_Compile_,
				Shader const& shader_,
				IDxcResult* shaderResult_
			) -> std::string {
				if (SUCCEEDED(hr_Compile_)) { return ""; }

				// Obtains the error messages from the compiler if available.
				std::string_view strView_shaderError{ "" };
				COMPtr<IDxcBlobUtf8> shaderError{ nullptr };
				shaderResult_->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
				if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
					strView_shaderError = shaderError->GetStringPointer();
				}

				std::string msg{
					std::format(
						"<DX12.Shader.Compiler - {}> Compilation failed!\n{}\n",
						shader_.DebugName(),
						strView_shaderError
					)
				};
				return msg;
			}
		};
		HRESULT hr_Compile{
			Wrapped_->Compile(
				&shaderSourceBuffer,
				args.data(),
				static_cast<uint32_t>(args.size()),
				IncludeHandler_,
				IID_PPV_ARGS(&shaderResult)
			)
		};
		hr_Compile ||
		Utils::Debug::ThrowIfFailed{
			msg_Failure(
				hr_Compile,
				shader_,
				shaderResult.Get()
			)
		};
		Logger().Message<0U>(
			"Shader.Compiler,{},Source compiled successfully.\n",
			shader_.DebugName()
		);

		HRESULT hr_GetBlob{
			shaderResult->GetOutput(
				DXC_OUT_OBJECT,
				IID_PPV_ARGS(shader_.GetAddressOf()),
				nullptr
			)
		};
		hr_GetBlob ||
		Utils::Debug::ThrowIfFailed{
			std::format(
				"<DX12.Shader.Compiler - {}> Failed to obtain the binary data of the compiled shader!\n",
				shader_.DebugName()
			)
		};
		Logger().Message<0U>(
			"Shader.Compiler,{},Binary data obtained successfully.\n",
			shader_.DebugName()
		);
	}

	//----	------	------	------	------	----//

	void Shader::Compiler::Initialize(std::string_view debugName_) {
		ThrowIfInitialized(debugName_);

		::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Wrapped_)) ||
		Utils::Debug::ThrowIfFailed{
			"<DX12.Shader.Compiler> Failed to create an HLSL compiler.\n"
		};
		Logger().Message<0U>("Shader::Compiler,,HLSL compiler created successfully.\n");

		::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils_)) ||
		Utils::Debug::ThrowIfFailed{
			"<DX12.Shader.Compiler> Failed to create DXC utilities.\n"
		};
		Logger().Message<0U>("Shader::Compiler,,DXC utilities created successfully.\n");

		Utils_->CreateDefaultIncludeHandler(&IncludeHandler_) ||
		Utils::Debug::ThrowIfFailed{
			"<DX12.Shader.Compiler> Failed to create an include handler.\n"
		};
		Logger().Message<0U>("Shader.Compiler,,Include handler created successfully.\n");

		SetDebugName(debugName_);
	}

	//----	------	------	------	------	----//

	constexpr Shader::Compiler::Compiler() noexcept {}

	Shader::Compiler::~Compiler() noexcept {
		IncludeHandler_->Release();
		Utils_->Release();
	}
}