module Lumina.Editor.DX12 : RootParameter;

import <cstdint>;

namespace Lumina::Editor {
	template<>
	class Module<DX12RootParameter> {
	public:
		constexpr auto Get() const noexcept -> D3D12_ROOT_PARAMETER const&;
	
	public:
		void Update(int idx_RootParam_);

	public:
		void Initialize();

	private:
		D3D12_ROOT_PARAMETER RootParameter_{};

		int32_t ComboUI_ItemIndex_ShaderVisibility_{};
		int32_t ComboUI_ItemIndex_RootParameterType_{};
		int32_t ComboUI_ItemIndex_DescriptorTable_{};

	private:
		static inline Lumina::Editor::Lexicon<D3D12_SHADER_VISIBILITY> const ShaderVisibilities_{
			{ "All", D3D12_SHADER_VISIBILITY_ALL },
			{ "Vertex", D3D12_SHADER_VISIBILITY_VERTEX },
			{ "Pixel", D3D12_SHADER_VISIBILITY_PIXEL },
		};
		static inline Lumina::Editor::Lexicon<D3D12_ROOT_PARAMETER_TYPE> const RootParameterTypes_{
			{ "DescriptorTable", D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE },
			{ "32BitConstants", D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS },
			{ "CBV", D3D12_ROOT_PARAMETER_TYPE_CBV },
			{ "SRV", D3D12_ROOT_PARAMETER_TYPE_SRV },
			{ "UAV", D3D12_ROOT_PARAMETER_TYPE_UAV },
		};
	};

	constexpr auto Module<DX12RootParameter>::Get()
		const noexcept -> D3D12_ROOT_PARAMETER const& { return RootParameter_; }

	void Module<DX12RootParameter>::Update(int idx_RootParam_) {
		ImGui::PushID(this);

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		{
			TableUI::AddRectFilledAtCurrentCellTopLeftCorner(
				0x0A1F2F07,
				0.0f,
				42.0f + ImGui::GetStyle().CellPadding.y
			);

			ImGui::PushID(this);
			ImGui::InvisibleButton("", ImVec2{ 16.0f, 42.0f });
			ImGui::PopID();

			ImVec2 const min_ItemRect{ ImGui::GetItemRectMin() };
			ImVec2 const max_ItemRect{ ImGui::GetItemRectMax() };
			ImVec2 const textPos{
				min_ItemRect.x,
				min_ItemRect.y + 21.0f - ImGui::GetFontSize() * 0.5f
			};
			ImVec4 const clipRect{
				min_ItemRect.x, min_ItemRect.y,
				max_ItemRect.x, max_ItemRect.y
			};

			ImDrawList* drawList{ ImGui::GetWindowDrawList() };
			drawList->AddText(
				ImGui::GetFont(),
				ImGui::GetFontSize(),
				textPos,
				0xFFFFFFFF,
				std::to_string(idx_RootParam_).data(),
				nullptr,
				0.0f,
				&clipRect
			);
		}

		ImGui::TableSetColumnIndex(1);
		{
			if (
				ComboUI::Begin(
					&ShaderVisibilities_,
					ShaderVisibilities_.Lexis(ComboUI_ItemIndex_ShaderVisibility_)
				)
			) {
				ComboUI::List(
					RootParameter_.ShaderVisibility,
					ComboUI_ItemIndex_ShaderVisibility_,
					ShaderVisibilities_
				);
				ComboUI::End();
			}	
		}

		//ImGui::TableSetColumnIndex(2);
		//{
		//	FillCell(0x0A1F2F07, 42.0f);

		//	auto paramType_Prev{ rsParam.ParameterType };

		//	if (
		//		Combo(
		//			rsParam.ParameterType,
		//			ComboCurrentItems_RootParameterType_.at(idx),
		//			RootParameterTypes_,
		//			"Root Parameter Type"
		//		)
		//	) {
		//		if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
		//			rsParam.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{};
		//		}
		//		else if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
		//			rsParam.Constants = D3D12_ROOT_CONSTANTS{
		//				.Num32BitValues{ 1U },
		//			};
		//		}
		//		// CBV, SRV, UAV
		//		else {
		//			if (
		//				(paramType_Prev == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) ||
		//				(paramType_Prev == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
		//			) {
		//				rsParam.Descriptor = D3D12_ROOT_DESCRIPTOR{};
		//			}
		//		}
		//	}
		//}

		//ImGui::TableSetColumnIndex(3);
		//{
		//	if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
		//		auto& item{ ComboCurrentItems_DescriptorTable_.at(idx) };

		//		char const* preview{ nullptr };
		//		if (item > -1) {
		//			preview = TableNames_.at(item).data();
		//		}
		//		if (
		//			ImGui::BeginCombo(
		//				"##Descriptor Table",
		//				preview
		//			)
		//		) {
		//			for (int i{ 0 }; i < static_cast<int>(RSDescriptorTables_.size()); ++i) {
		//				bool const isSelected{ (item == i) };
		//				if (ImGui::Selectable(TableNames_.at(i).data(), isSelected)) {
		//					item = i;
		//				}
		//				if (isSelected) {
		//					ImGui::SetItemDefaultFocus();
		//				}
		//			}
		//			ImGui::EndCombo();
		//		}

		//		if (item > -1) {
		//			rsParam.DescriptorTable.pDescriptorRanges =
		//				RSDescriptorTables_.at(item).data();
		//			rsParam.DescriptorTable.NumDescriptorRanges =
		//				static_cast<uint32_t>(RSDescriptorTables_.at(item).size());
		//		}
		//	}
		//	else if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
		//		SetRootConstants(rsParam.Constants);
		//	}
		//	// CBV, SRV, UAV
		//	else {
		//		SetRootDescriptor(rsParam.Descriptor, rsParam.ParameterType);
		//	}
		//}

		ImGui::PopID();
	}

	void Module<DX12RootParameter>::Initialize() {

	}
}

namespace Lumina::Editor {
	namespace {
		void DecorativeRow() {
			ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, 0x3F + (0x9F << 8) + (0x7F << 16) + (0x1F << 24));
			ImGui::PushStyleVarY(ImGuiStyleVar_CellPadding, 2.0f);
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
		}
	}

	void Module<DX12RootParameterList>::SetupTableUIColumns() {
		ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 16.0f);
		ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 64.0f);
		ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 104.0f);
	}

	void Module<DX12RootParameterList>::SetupTableUIHeaderRow() {
		ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

		ImGui::TableSetColumnIndex(0);
		TableUI::AddRectFilledAtCurrentCellTopLeftCorner(
			0x0A1F2F07,
			0.0f,
			24.0f + ImGui::GetStyle().CellPadding.y
		);

		for (int idx_Col{ 0 }; idx_Col < 4; ++idx_Col) {
			ImGui::TableSetColumnIndex(idx_Col);
			{
				ImGui::PushID(idx_Col);
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
				/*if (ImGui::TableGetHoveredColumn() == idx_Col) {
					if (ImGui::BeginTooltip()) {
						ImGui::Text(TableUI_ColumnTooltipTexts_[idx_Col].data());
						ImGui::EndTooltip();
					}
				}*/
				ImGui::TableHeader(TableUI_ColumnNames_[idx_Col].data());
				ImGui::PopID();
			}
		}

	}

	void Module<DX12RootParameterList>::Update() {
		if (
			PanelUI::Begin(
				this,
				Vec2{},
				ImGuiChildFlags_Borders,
				ImGuiWindowFlags_MenuBar
			)
		) {
			if (ImGui::BeginMenuBar()) {
				if (ImGui::MenuItem("New Parameter")) {
					auto& rootParam{ Modules_RootParameter_.emplace_back(new Module<DX12RootParameter>{}) };
					rootParam->Initialize();
				}
				ImGui::EndMenuBar();
			}

			if (
				TableUI::Begin(
					this,
					4, Vec2{}, 0.0f,
					ImGuiTableFlags_BordersOuter |
					ImGuiTableFlags_BordersInnerH |
					ImGuiTableFlags_RowBg
				)
			) {
				SetupTableUIColumns();
				DecorativeRow();
				SetupTableUIHeaderRow();

				int idx{ 0 };
				for (auto& rootParam : Modules_RootParameter_) {
					rootParam->Update(idx++);
				}

				DecorativeRow();

				TableUI::End();
			}

			PanelUI::End();
		}
	}

	void Module<DX12RootParameterList>::Initialize() {
		auto& rootParam{ Modules_RootParameter_.emplace_back(new Module<DX12RootParameter>{}) };
		rootParam->Initialize();
	}
}