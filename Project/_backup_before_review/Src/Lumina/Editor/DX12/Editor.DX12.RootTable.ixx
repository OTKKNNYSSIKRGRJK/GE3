export module Lumina.Editor.DX12 : RootTable;

import <memory>;

import <vector>;

import <string>;
import <format>;

import : ModuleName;

import Lumina.DX12;

import Lumina.Editor;

namespace Lumina::Editor {
	template<>
	class Module<DX12RootTable>;
	template<>
	class Module<DX12RootTableList>;
}

namespace Lumina::Editor {
	template<>
	class Module<DX12RootTableList> {
	public:
		constexpr auto TableUI_ColumnProportionalWidth(uint32_t idx_) -> float&;

		static constexpr auto TableUI_ColumnName(uint32_t idx_) -> std::string_view;
		static constexpr auto TableUI_ColumnTooltipText(uint32_t idx_) -> std::string_view;

	private:
		void SetupTableUIColumns();
		void SetupTableUIHeaderRow();

	public:
		void Update();

	public:
		void Initialize();
		void Finalize() noexcept;

	public:
		constexpr Module() noexcept;
		~Module() noexcept;

	private:
		std::vector<Module<DX12RootTable>*> Modules_RootTable_{};

		float TableUI_ColumnProportionalWidths_[5]{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, };

	private:
		static constexpr std::string_view TableUI_ColumnNames_[5]{
			"Range\nType",
			"Range\nSize",
			"Base\nRegister",
			"Register\nSpace",
			"Offset",
		};
		static constexpr std::string_view TableUI_ColumnTooltipTexts_[5]{
			"Type of the descriptor range",
			"Number of descriptors in the range",
			"First register to which the first descriptor is bound",
			"Register space of the descriptors in the range",
			"Offset in descriptors in the range\nrelative to the first descriptor in this root table;\nCan be D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND (0xFFFFFFFF)\nif this range should immediately follow the preceding one",
		};
	};
}