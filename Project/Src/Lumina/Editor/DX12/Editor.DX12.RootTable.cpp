module Lumina.Editor.DX12 : RootTable;

namespace Lumina::Editor {
	template<>
	class Module<DX12RootTable> {
	public:
		constexpr auto Get() const noexcept -> D3D12_ROOT_DESCRIPTOR_TABLE;

	private:
		void Widget_Rename();
		void Widget_NewRange();

		void SetupTableUIColumns(Module<DX12RootTableList>& rootTableList_);

	public:
		void Update(Module<DX12RootTableList>& rootTableList_);

	public:
		void Initialize(std::string_view name_ = "Table");

	public:
		constexpr Module() noexcept = default;
		constexpr ~Module() noexcept = default;

	private:
		std::vector<D3D12_DESCRIPTOR_RANGE> Ranges_{};

		std::string Name_{};

		int32_t TreeNodeUI_IsOpen_{ 1 };
		std::string InputTextUI_Buffer_Name_{};
		std::vector<int32_t> ComboUI_ItemIndices_RangeType_{};

	private:
		static constexpr Int2 DragScalarUI_Range_RangeSize_{ 1, 65535 };
		static constexpr Int2 DragScalarUI_Range_BaseRegister_{ 0, 65535 };
		static constexpr Int2 DragScalarUI_Range_RegisterSpace_{ 0, 65535 };
		static constexpr Int2 DragScalarUI_Range_Offset_{ -1, 65535 };
		static constexpr Int2 const* DragScalarUI_Ranges_[4]{
			&DragScalarUI_Range_RangeSize_,
			&DragScalarUI_Range_BaseRegister_,
			&DragScalarUI_Range_RegisterSpace_,
			&DragScalarUI_Range_Offset_
		};

		static constexpr auto DragScalarUI_TextFormat_BaseRegister_{
			[](D3D12_DESCRIPTOR_RANGE_TYPE rangeType_) -> std::string_view const& {
				static constexpr std::string_view fmt_SRV{ "t%u" };
				static constexpr std::string_view fmt_UAV{ "u%u" };
				static constexpr std::string_view fmt_CBV{ "b%u" };
				static constexpr std::string_view fmt_Default{ "%u" };

				if (rangeType_ == D3D12_DESCRIPTOR_RANGE_TYPE_SRV) { return fmt_SRV; }
				if (rangeType_ == D3D12_DESCRIPTOR_RANGE_TYPE_UAV) { return fmt_UAV; }
				if (rangeType_ == D3D12_DESCRIPTOR_RANGE_TYPE_CBV) { return fmt_CBV; }
				return fmt_Default;
			}
		};
		static constexpr std::string_view DragScalarUI_TextFormat_RangeSize_{ "%u" };
		static constexpr std::string_view DragScalarUI_TextFormat_RegisterSpace_{ "space%u" };
		static constexpr std::string_view DragScalarUI_TextFormat_Offset_{ "%d" };

		static inline Lexicon<D3D12_DESCRIPTOR_RANGE_TYPE> const DescriptorRangeTypes_{
			{ "SRV", D3D12_DESCRIPTOR_RANGE_TYPE_SRV },
			{ "UAV", D3D12_DESCRIPTOR_RANGE_TYPE_UAV },
			{ "CBV", D3D12_DESCRIPTOR_RANGE_TYPE_CBV },
		};
	};

	constexpr auto Module<DX12RootTable>::Get()
		const noexcept -> D3D12_ROOT_DESCRIPTOR_TABLE {
		return D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges{ static_cast<uint32_t>(Ranges_.size()) },
			.pDescriptorRanges{ Ranges_.data() },
		};
	}

	void Module<DX12RootTable>::Widget_Rename() {
		ImGui::PushID("Rename");
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
		ImGui::InputTextEx(
			"",
			Name_.data(),
			InputTextUI_Buffer_Name_.data(),
			128,
			ImVec2{},
			ImGuiInputTextFlags_None
		);
		ImGui::PopItemWidth();
		ImGui::PopID();

		if (ImGui::GetActiveID() != ImGui::GetItemID()) {
			InputTextUI_Buffer_Name_ = InputTextUI_Buffer_Name_.data();
			if (InputTextUI_Buffer_Name_.length() > 0LLU) {
				Name_ = InputTextUI_Buffer_Name_;
			}
			else {
				InputTextUI_Buffer_Name_ = Name_;
			}
		}
	}

	void Module<DX12RootTable>::Widget_NewRange() {
		if (ImGui::Button("New Range")) {
			auto& newRange{ Ranges_.emplace_back() };
			{
				newRange.NumDescriptors = 1U;
				newRange.OffsetInDescriptorsFromTableStart = 0xFFFFFFFFU;
			}
			ComboUI_ItemIndices_RangeType_.emplace_back();
		}
	}

	void Module<DX12RootTable>::SetupTableUIColumns(Module<DX12RootTableList>& rootTableList_) {
		for (int32_t idx_Col{ 0 }; idx_Col < 5; ++idx_Col) {
			ImGui::TableSetupColumn(
				Module<DX12RootTableList>::TableUI_ColumnName(idx_Col).data(),
				ImGuiTableColumnFlags_WidthStretch,
				rootTableList_.TableUI_ColumnProportionalWidth(idx_Col)
			);
		}
	}

	void Module<DX12RootTable>::Update(Module<DX12RootTableList>& rootTableList_) {
		ImGuiStyle const& style{ ImGui::GetStyle() };

		ImGui::SetNextItemOpen(TreeNodeUI_IsOpen_);
		if (
			TreeNodeUI<false>::Begin(
				&Ranges_,
				"",
				ImGuiTreeNodeFlags_Framed |
				ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_AllowOverlap
			)
		) {
			if (ImGui::IsItemToggledOpen()) {
				TreeNodeUI_IsOpen_ ^= 1;
			}

			ImGui::SameLine(style.IndentSpacing + style.FramePadding.x * 2.0f);
			Widget_Rename();

			ImGui::SameLine();
			Widget_NewRange();

			if (
				TableUI::Begin(
					&Ranges_,
					5, Vec2{}, 0.0f,
					ImGuiTableFlags_BordersOuter |
					ImGuiTableFlags_BordersInnerH |
					ImGuiTableFlags_RowBg
				)
			) {
				SetupTableUIColumns(rootTableList_);

				ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, 0x3F + (0x9F << 8) + (0x7F << 16) + (0x1F << 24));
				ImGui::PushStyleVarY(ImGuiStyleVar_CellPadding, 2.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
				for (int idx_Col{ 0 }; idx_Col < 5; ++idx_Col) {
					ImGui::TableSetColumnIndex(idx_Col);
					ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
				}
				ImGui::PopStyleVar();

				for (auto& range : Ranges_) {
					auto idx_Range{ &range - Ranges_.data() };

					ImGui::PushID(static_cast<int>(idx_Range));

					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					{
						ImGui::PushID(ImGui::TableGetColumnName(0));
						auto& item{ ComboUI_ItemIndices_RangeType_.at(idx_Range) };
						if (ImGui::BeginCombo("", nullptr, ImGuiComboFlags_CustomPreview)) {
							for (
								int32_t idx_ComboItem{ 0 };
								idx_ComboItem < static_cast<int>(DescriptorRangeTypes_.Size());
								++idx_ComboItem
							) {
								ImGui::PushID(idx_ComboItem);
								bool const isSelected{ (item == idx_ComboItem) };
								if (
									ImGui::Selectable(
										DescriptorRangeTypes_.Lexis(idx_ComboItem).data(),
										isSelected
									)
								) {
									//if (item != idx_ComboItem) { ret = true; }
									item = idx_ComboItem;
									range.RangeType = DescriptorRangeTypes_.Content(idx_ComboItem);
								}
								if (isSelected) {
									ImGui::SetItemDefaultFocus();
								}
								ImGui::PopID();
							}

							ImGui::EndCombo();
						}
						if (ImGui::BeginComboPreview()) {
							ImGui::Text(DescriptorRangeTypes_.Lexis(item).data());
							ImGui::EndComboPreview();
						}
						ImGui::PopID();
					}

					std::string_view const* const fmts[4]{
						&DragScalarUI_TextFormat_RangeSize_,
						&DragScalarUI_TextFormat_BaseRegister_(range.RangeType),
						&DragScalarUI_TextFormat_RegisterSpace_,
						&DragScalarUI_TextFormat_Offset_,
					};

					for (int32_t idx_Col{ 1 }; idx_Col < 5; ++idx_Col) {
						ImGui::TableSetColumnIndex(idx_Col);
						{
							ImGui::PushID(ImGui::TableGetColumnName(idx_Col));
							ImGui::PushItemFlag(ImGuiItemFlags_Disabled, (idx_Range == 0 && idx_Col == 4));
							ImGui::DragScalar(
								"",
								ImGuiDataType_S32,
								&range.NumDescriptors + (idx_Col - 1),
								0.0625f,
								&DragScalarUI_Ranges_[(idx_Col - 1)]->x,
								&DragScalarUI_Ranges_[(idx_Col - 1)]->y,
								fmts[(idx_Col - 1)]->data(),
								ImGuiSliderFlags_AlwaysClamp
							);
							ImGui::PopItemFlag();
							ImGui::PopID();
						}
					}

					for (int idx_Col{ 0 }; idx_Col < 5; ++idx_Col) {
						ImGui::TableSetColumnIndex(idx_Col);
						{
							if (
								(ImGui::TableGetHoveredRow() == ImGui::TableGetRowIndex()) &&
								(ImGui::TableGetHoveredColumn() == idx_Col)
							) {
								TableUI::AddRectFilledAtCurrentCellTopLeftCorner(
									0x1A2F4F0F,
									0.0f,
									ImGui::GetFontSize() + style.CellPadding.y * 2.0f
								);
							}
						}
					}

					ImGui::PopID();
				}

				ImGui::PushStyleVarY(ImGuiStyleVar_CellPadding, 2.0f);
				ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();

				TableUI::End();
			}

			TreeNodeUI<false>::End();
		}
		else {
			if (ImGui::IsItemToggledOpen()) {
				TreeNodeUI_IsOpen_ ^= 1;
			}
			ImGui::SameLine(style.IndentSpacing + style.FramePadding.x * 3.0f);
			ImGui::Text(Name_.data());
		}
	}

	void Module<DX12RootTable>::Initialize(std::string_view name_) {
		Name_ = name_;

		InputTextUI_Buffer_Name_.resize(128LLU);
		InputTextUI_Buffer_Name_ = Name_;

		auto& newRange{ Ranges_.emplace_back() };
		newRange.NumDescriptors = 1U;

		ComboUI_ItemIndices_RangeType_.emplace_back();
	}
}

namespace Lumina::Editor {
	constexpr auto Module<DX12RootTableList>::TableUI_ColumnProportionalWidth(uint32_t idx_)
		-> float& {
		return TableUI_ColumnProportionalWidths_[idx_];
	}

	constexpr auto Module<DX12RootTableList>::TableUI_ColumnName(uint32_t idx_)
		-> std::string_view {
		return TableUI_ColumnNames_[idx_];
	}
	constexpr auto Module<DX12RootTableList>::TableUI_ColumnTooltipText(uint32_t idx_)
		-> std::string_view {
		return TableUI_ColumnTooltipTexts_[idx_];
	}

	void Module<DX12RootTableList>::SetupTableUIColumns() {
		for (int32_t idx_Col{ 0 }; idx_Col < 5; ++idx_Col) {
			ImGui::TableSetupColumn(
				TableUI_ColumnNames_[idx_Col].data(),
				ImGuiTableColumnFlags_WidthStretch,
				TableUI_ColumnProportionalWidths_[idx_Col]
			);
		}
	}

	void Module<DX12RootTableList>::SetupTableUIHeaderRow() {
		ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

		for (int idx_Col{ 0 }; idx_Col < 5; ++idx_Col) {
			ImGui::TableSetColumnIndex(idx_Col);
			{
				ImGui::PushID(idx_Col);
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
				if (ImGui::TableGetHoveredColumn() == idx_Col) {
					if (ImGui::BeginTooltip()) {
						ImGui::Text(TableUI_ColumnTooltipTexts_[idx_Col].data());
						ImGui::EndTooltip();
					}
				}
				ImGui::TableHeader(ImGui::TableGetColumnName(idx_Col));
				ImGui::PopID();
			}
		}
	}

	void Module<DX12RootTableList>::Update() {
		if (
			PanelUI::Begin(
				this,
				Vec2{},
				ImGuiChildFlags_Borders,
				ImGuiWindowFlags_MenuBar
			)
		) {
			if (ImGui::BeginMenuBar()) {
				if (ImGui::MenuItem("New Table")) {
					auto& rootTable{ Modules_RootTable_.emplace_back(new Module<DX12RootTable>{}) };
					rootTable->Initialize();
				}
				ImGui::EndMenuBar();
			}

			if (
				TableUI::Begin(
					this,
					5, Vec2{}, 0.0f,
					ImGuiTableFlags_BordersOuter |
					ImGuiTableFlags_BordersInnerH
				)
			) {
				SetupTableUIColumns();
				SetupTableUIHeaderRow();

				TableUI::End();
			}

			for (auto& rootTable : Modules_RootTable_) {
				ImGui::PushStyleColor(ImGuiCol_Separator, 0x00000000);
				ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 4.0f);
				ImGui::PopStyleColor();

				rootTable->Update(*this);
			}

			PanelUI::End();
		}
	}

	void Module<DX12RootTableList>::Initialize() {
		auto& rootTable{ Modules_RootTable_.emplace_back(new Module<DX12RootTable>{}) };
		rootTable->Initialize();
	}

	void Module<DX12RootTableList>::Finalize() noexcept {
		for (auto& rootTable : Modules_RootTable_) {
			if (rootTable != nullptr) {
				delete rootTable;
				rootTable = nullptr;
			}
		}
		Modules_RootTable_.clear();
	}

	constexpr Module<DX12RootTableList>::Module() noexcept {}

	Module<DX12RootTableList>::~Module() noexcept { Finalize(); }
}