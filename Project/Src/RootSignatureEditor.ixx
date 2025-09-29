module;

#include <d3d12.h>

export module RootSignatureEditor;

import <type_traits>;

import <algorithm>;

import <utility>;
import <initializer_list>;

import <vector>;
import <set>;
import <unordered_set>;

import <string>;
import <format>;

import Lumina.Math.Numerics;
import Lumina.Math.Vector;

import Lumina.DX12;

import Lumina.Utils.ImGui;

import Lumina.Editor;

constinit uint32_t const Step_U32{ 1U };
constinit uint32_t const StepFast_U32{ 100U };
constinit uint32_t const Min_Register{ 0x0000U };
constinit uint32_t const Max_Register{ 0xFFFFU };
constinit uint32_t const Min_RegisterSpace{ 0x0000U };
constinit uint32_t const Max_RegisterSpace{ 0xFFFFU };
constinit uint32_t const Min_Num_32BitConstants{ 0x01U };
constinit uint32_t const Max_Num_32BitConstants{ 0x10U };

bool NonCollapsibleTreeNode(char const* label_) {
	return ImGui::TreeNodeEx(
		label_,
		ImGuiTreeNodeFlags_DrawLinesToNodes |
		ImGuiTreeNodeFlags_Leaf |
		ImGuiTreeNodeFlags_Bullet
	);
}

void FillCell(uint32_t rgba_, float height_) {
	ImGuiContext const* context{ ImGui::GetCurrentContext() };
	ImGuiWindow const* window{ context->CurrentWindow };

	auto const& style{ ImGui::GetStyle() };
	ImVec2 const min_RectFilled{
		window->DC.CursorPos.x - style.CellPadding.x,
		window->DC.CursorPos.y - style.CellPadding.y
	};
	ImVec2 const max_RectFilled{
		window->WorkRect.Max.x + style.CellPadding.x,
		window->DC.CursorPos.y + height_ + style.CellPadding.y
	};

	ImDrawList* drawList{ ImGui::GetWindowDrawList() };
	drawList->AddRectFilled(min_RectFilled, max_RectFilled, rgba_);
}

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

export class RootSignatureEditor {
private:
	void AppendParameter() {
		if (ImGui::MenuItem("Append Parameter")) {
			RSParameters_.emplace_back();
			ComboCurrentItems_RootParameterType_.emplace_back();
			ComboCurrentItems_ShaderVisibility_.emplace_back();
			ComboCurrentItems_DescriptorTable_.emplace_back(-1);
		}
	}

	template<typename T>
	bool Combo(
		T& content_,
		int& item_,
		Lumina::Editor::Lexicon<T> const& lexicon_,
		char const* label_
	) {
		bool ret{ false };

		if (
			ImGui::BeginCombo(
				std::format("##{}", label_).data(),
				lexicon_.Lexis(item_).data()
			)
		) {
			for (int i{ 0 }; i < static_cast<int>(lexicon_.Size()); ++i) {
				bool const isSelected{ (item_ == i) };
				ImGui::PushID(i);
				ImGui::SetNextItemAllowOverlap();
				if (ImGui::Selectable(lexicon_.Lexis(i).data(), isSelected)) {
					if (item_ != i) { ret = true; }
					item_ = i;
					content_ = lexicon_.Content(i);
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		return ret;
	}
	
	void SetRootDescriptor(
		D3D12_ROOT_DESCRIPTOR& rootDescriptor_,
		D3D12_ROOT_PARAMETER_TYPE rootParamType_
	) {
		char fmt_RegisterName[4]{ " %u" };
		if (rootParamType_ == D3D12_ROOT_PARAMETER_TYPE_CBV) {
			fmt_RegisterName[0] = 'b';
		}
		else if (rootParamType_ == D3D12_ROOT_PARAMETER_TYPE_SRV) {
			fmt_RegisterName[0] = 't';
		}
		else if (rootParamType_ == D3D12_ROOT_PARAMETER_TYPE_UAV) {
			fmt_RegisterName[0] = 'u';
		}

		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);

		ImGui::PushID("Register");
		ImGui::DragScalar(
			"",
			ImGuiDataType_U32,
			&rootDescriptor_.ShaderRegister,
			0.0625f,
			&Min_Register,
			&Max_Register,
			fmt_RegisterName,
			ImGuiSliderFlags_AlwaysClamp
		);
		if (ImGui::BeginItemTooltip()) {
			ImGui::Text("Register");
			ImGui::EndTooltip();
		}
		ImGui::PopID();

		ImGui::PushID("Register Space");
		ImGui::DragScalar(
			"",
			ImGuiDataType_U32,
			&rootDescriptor_.RegisterSpace,
			0.0625f,
			&Min_RegisterSpace,
			&Max_RegisterSpace,
			"space%u",
			ImGuiSliderFlags_AlwaysClamp
		);
		if (ImGui::BeginItemTooltip()) {
			ImGui::Text("Register Space");
			ImGui::EndTooltip();
		}
		ImGui::PopID();

		ImGui::PopItemWidth();
	}

	void SetRootConstants(D3D12_ROOT_CONSTANTS& rootConstants_) {
		/*NonCollapsibleTreeNode("Register");
		ImGui::DragScalar(
			"##Register",
			ImGuiDataType_U32,
			&rootConstants_.ShaderRegister,
			0.0625f,
			&Min_Register,
			&Max_Register,
			"b%u",
			ImGuiSliderFlags_AlwaysClamp
		);
		ImGui::TreePop();

		NonCollapsibleTreeNode("Register Space");
		ImGui::DragScalar(
			"##Register Space",
			ImGuiDataType_U32,
			&rootConstants_.RegisterSpace,
			0.0625f,
			&Min_RegisterSpace,
			&Max_RegisterSpace,
			"space%u",
			ImGuiSliderFlags_AlwaysClamp
		);
		ImGui::TreePop();

		NonCollapsibleTreeNode("Number of 32-bit Constant(s)");
		ImGui::DragScalar(
			"##Number of 32-bit Constant(s)",
			ImGuiDataType_U8,
			&rootConstants_.Num32BitValues,
			0.0625f,
			&Min_Num_32BitConstants,
			&Max_Num_32BitConstants,
			"%u",
			ImGuiSliderFlags_AlwaysClamp
		);
		ImGui::TreePop();*/

		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);

		ImGui::BeginGroup();
		ImGui::PushID("Register");
		ImGui::DragScalar(
			"",
			ImGuiDataType_U32,
			&rootConstants_.ShaderRegister,
			0.0625f,
			&Min_Register,
			&Max_Register,
			"b%u",
			ImGuiSliderFlags_AlwaysClamp
		);
		if (ImGui::BeginItemTooltip()) {
			ImGui::Text("Register");
			ImGui::EndTooltip();
		}
		ImGui::PopID();

		ImGui::PushID("Register Space");
		ImGui::DragScalar(
			"",
			ImGuiDataType_U32,
			&rootConstants_.RegisterSpace,
			0.0625f,
			&Min_RegisterSpace,
			&Max_RegisterSpace,
			"space%u",
			ImGuiSliderFlags_AlwaysClamp
		);
		if (ImGui::BeginItemTooltip()) {
			ImGui::Text("Register Space");
			ImGui::EndTooltip();
		}
		ImGui::PopID();
		ImGui::EndGroup();

		ImGui::PopItemWidth();

		ImGui::SameLine();

		// Width of the content region available = width of the cell - width of the previous group
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

		ImGui::PushID("Number of 32-bit Constant(s)");
		ImGui::DragScalar(
			"",
			ImGuiDataType_U8,
			&rootConstants_.Num32BitValues,
			0.0625f,
			&Min_Num_32BitConstants,
			&Max_Num_32BitConstants,
			"Num: %u",
			ImGuiSliderFlags_AlwaysClamp
		);
		if (ImGui::BeginItemTooltip()) {
			ImGui::Text("Number of 32-bit Constant(s)");
			ImGui::EndTooltip();
		}
		ImGui::PopID();

		ImGui::PopItemWidth();
	}

	void SetupRootParameterTableColumns() {
		ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 16.0f);
		ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 64.0f);
		ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, 104.0f);

		ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

		ImGui::TableSetColumnIndex(0);
		{
			FillCell(0x1A2F4F0F, 24.0f);

			// PushID is used to ensure every column's identifier is unique.
			// For more details, refer to ImGui::TableHeadersRow().
			ImGui::PushID(0);
			ImGui::TableHeader("#");
			ImGui::PopID();
		}

		ImGui::TableSetColumnIndex(1);
		{
			ImGui::PushID(1);
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::TableHeader("Shader\nVisibility");
			ImGui::PopID();
		}

		ImGui::TableSetColumnIndex(2);
		{
			FillCell(0x1A2F4F0F, 24.0f);

			ImGui::PushID(2);
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::TableHeader("Parameter\nType");
			ImGui::PopID();
		}

		ImGui::TableSetColumnIndex(3);
		{
			ImGui::PushID(3);
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::TableHeader("Parameter\nDetails");
			ImGui::PopID();
		}
	}

	void SetRootParameters() {
		ImGui::BeginChild("Tables", ImVec2{}, ImGuiChildFlags_Borders, ImGuiWindowFlags_MenuBar);

		ImGui::BeginMenuBar();
		AppendParameter();
		ImGui::EndMenuBar();

		if(
			ImGui::BeginTable(
				"RootParameters",
				4,
				ImGuiTableFlags_BordersOuter |
				ImGuiTableFlags_BordersInnerH |
				ImGuiTableFlags_RowBg
			)
		) {
			SetupRootParameterTableColumns();

			for (auto& rsParam : RSParameters_) {
				auto idx{ &rsParam - RSParameters_.data() };

				ImGui::PushID(std::format("##RSParam#{}", idx).data());

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				{
					FillCell(0x0A1F2F07, 42.0f);

					ImGui::PushID(static_cast<int>(idx));
					ImGui::InvisibleButton("", ImVec2{ 16.0f, 42.0f });
					ImGui::PopID();

					ImVec2 const min_ItemRect{ ImGui::GetItemRectMin() };
					ImVec2 const max_ItemRect{ ImGui::GetItemRectMax() };
					ImVec2 const textPos{ min_ItemRect.x, min_ItemRect.y + 21.0f - 6.0f };
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
						std::to_string(idx).data(),
						nullptr,
						0.0f,
						&clipRect
					);
				}

				ImGui::TableSetColumnIndex(1);
				Combo(
					rsParam.ShaderVisibility,
					ComboCurrentItems_ShaderVisibility_.at(idx),
					ShaderVisibilities_,
					"Shader Visibility"
				);

				ImGui::TableSetColumnIndex(2);
				{
					FillCell(0x0A1F2F07, 42.0f);

					auto paramType_Prev{ rsParam.ParameterType };

					if (
						Combo(
							rsParam.ParameterType,
							ComboCurrentItems_RootParameterType_.at(idx),
							RootParameterTypes_,
							"Root Parameter Type"
						)
					) {
						if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
							rsParam.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{};
						}
						else if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
							rsParam.Constants = D3D12_ROOT_CONSTANTS{
								.Num32BitValues{ 1U },
							};
						}
						// CBV, SRV, UAV
						else {
							if (
								(paramType_Prev == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) ||
								(paramType_Prev == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
							) {
								rsParam.Descriptor = D3D12_ROOT_DESCRIPTOR{};
							}
						}
					}
				}

				ImGui::TableSetColumnIndex(3);
				{
					if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
						auto& item{ ComboCurrentItems_DescriptorTable_.at(idx) };

						char const* preview{ nullptr };
						if (item > -1) {
							preview = TableNames_.at(item).data();
						}
						if (
							ImGui::BeginCombo(
								"##Descriptor Table",
								preview
							)
						) {
							for (int i{ 0 }; i < static_cast<int>(RSDescriptorTables_.size()); ++i) {
								bool const isSelected{ (item == i) };
								if (ImGui::Selectable(TableNames_.at(i).data(), isSelected)) {
									item = i;
								}
								if (isSelected) {
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}

						if (item > -1) {
							rsParam.DescriptorTable.pDescriptorRanges =
								RSDescriptorTables_.at(item).data();
							rsParam.DescriptorTable.NumDescriptorRanges =
								static_cast<uint32_t>(RSDescriptorTables_.at(item).size());
						}
					}
					else if (rsParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
						SetRootConstants(rsParam.Constants);
					}
					// CBV, SRV, UAV
					else {
						SetRootDescriptor(rsParam.Descriptor, rsParam.ParameterType);
					}
				}

				ImGui::PopID();

				//if (
				//	ImGui::TreeNodeEx(
				//		std::format("#{}", idx).data(),
				//		ImGuiTreeNodeFlags_DrawLinesToNodes |
				//		ImGuiTreeNodeFlags_DefaultOpen |
				//		ImGuiTreeNodeFlags_Bullet
				//	)
				//) {
				//	if (NonCollapsibleTreeNode("Shader Visibility")) {
				//		// ...
				//		ImGui::TreePop();
				//	}
				//	if (NonCollapsibleTreeNode("Root Parameter Type")) {
				//		// ...
				//		ImGui::TreePop();
				//	}

				//	ImGui::TreePop();
				//}
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();
	}

	void SetDescriptorRangeTableColumns() {
		ImGui::TableSetupColumn("Range\nType");
		ImGui::TableSetupColumn("Range\nSize");
		ImGui::TableSetupColumn("Base\nRegister");
		ImGui::TableSetupColumn("Register\nSpace");
		ImGui::TableSetupColumn("Offset");

		static std::string const toolTipTexts[5]{
			"Type of the descriptor range",
			"Number of descriptors in the range",
			"First register to which the first descriptor is bound",
			"Register space of the descriptors in the range",
			"Offset in descriptors in the range\nrelative to the first descriptor in this root table;\nCan be D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND (0xFFFFFFFF)\nif this range should immediately follow the preceding one",
		};
		
		ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

		for (int idx_Col{ 0 }; idx_Col < 5; ++idx_Col) {
			ImGui::TableSetColumnIndex(idx_Col);
			{
				ImGui::PushID(idx_Col);
				ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
				if (ImGui::TableGetHoveredColumn() == idx_Col) {
					if (ImGui::BeginTooltip()) {
						ImGui::Text(toolTipTexts[idx_Col].data());
						ImGui::EndTooltip();
					}
				}
				ImGui::TableHeader(ImGui::TableGetColumnName(idx_Col));
				ImGui::PopID();
			}
		}
	}

	void SetDescriptorTables() {}

public:
	void Update() {
		if (ImGui::BeginTabItem("Root Signature")) {
			ImGui::BeginChild(
				"Root Signature",
				ImVec2{},
				ImGuiChildFlags_Borders,
				ImGuiWindowFlags_NoCollapse
			);
			if (
				ImGui::BeginTabBar(
					"Root Signature",
					ImGuiTabBarFlags_FittingPolicyScroll |
					ImGuiTabBarFlags_TabListPopupButton
				)
			) {
				if (ImGui::BeginTabItem("Root Parameters")) {
					SetRootParameters();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Descriptor Tables")) {
					SetDescriptorTables();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
	}

private:
	std::vector<D3D12_ROOT_PARAMETER> RSParameters_{};
	std::vector<std::string> RSParameterTexts_{};
	std::vector<int> ComboCurrentItems_ShaderVisibility_{};
	std::vector<int> ComboCurrentItems_RootParameterType_{};
	std::vector<int> ComboCurrentItems_DescriptorTable_{};
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> RSDescriptorTables_{};
	std::vector<std::vector<int>> ComboCurrentItems_RangeType_{};
	std::vector<std::string> TableNames_{};
	std::vector<std::string> TableNameInputBuffers_{};
	std::vector<D3D12_STATIC_SAMPLER_DESC> RSStaticSamplers_{};

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
	static inline Lumina::Editor::Lexicon<D3D12_DESCRIPTOR_RANGE_TYPE> const DescriptorRangeTypes_{
		{ "SRV", D3D12_DESCRIPTOR_RANGE_TYPE_SRV },
		{ "UAV", D3D12_DESCRIPTOR_RANGE_TYPE_UAV },
		{ "CBV", D3D12_DESCRIPTOR_RANGE_TYPE_CBV },
	};
};