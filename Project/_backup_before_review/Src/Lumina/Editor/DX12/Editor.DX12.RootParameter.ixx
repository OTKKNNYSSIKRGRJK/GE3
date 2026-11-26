export module Lumina.Editor.DX12 : RootParameter;

import <vector>;

import <string>;

import : ModuleName;

import Lumina.DX12;

import Lumina.Editor;

namespace Lumina::Editor {
	template<>
	class Module<DX12RootParameterList> {
	private:
		void SetupTableUIColumns();
		void SetupTableUIHeaderRow();

	public:
		void Update();

	public:
		void Initialize();

	private:
		std::vector<Module<DX12RootParameter>*> Modules_RootParameter_{};

	private:
		static constexpr std::string_view TableUI_ColumnNames_[4]{
			"#",
			"Shader\nVisibility",
			"Parameter\nType",
			"Parameter\nDetails",
		};
	};
}