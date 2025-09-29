export module Lumina.Editor.DX12 : RootSignature;

import <memory>;

import : RootParameter;
import : RootTable;

import : ModuleName;

import Lumina.DX12;

import Lumina.Editor;

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

namespace Lumina::Editor {
	export template<>
	class Module<DX12RootSignature> final {
		using SelfType = Module<DX12RootSignature>;

	public:
		void Update();

	public:
		void Initialize();

	public:
		static auto Create() -> UniPtr<SelfType>;

	public:
		constexpr Module() noexcept = default;
		constexpr ~Module() noexcept = default;

	private:
		DX12::RootSignature::Setup RootSignatureSetup_{};

		Module<DX12RootParameterList> Module_RootParameterList_{};
		Module<DX12RootTableList> Module_RootTableList_{};
	};
}