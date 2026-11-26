export module Lumina.Editor;

import <cstdint>;
import <type_traits>;

import <string>;

export import : Lexicon;

export import Lumina.Utils.ImGui;

export import Lumina.Math.Numerics;
export import Lumina.Math.Vector;

namespace Lumina::Editor {
	export template<typename>
	class Module;
}

namespace Lumina::Editor {
	export class PanelUI {
	public:
		static inline bool Begin(
			void const* ptr_ID_,
			Vec2 const& size_ = Vec2{},
			ImGuiChildFlags flags_ChildWindow_ = ImGuiChildFlags_None,
			ImGuiWindowFlags flags_Window_ = ImGuiWindowFlags_None
		) {
			ImGui::PushID(ptr_ID_);
			return ImGui::BeginChild(
				"PanelUI",
				ImVec2{ size_.x, size_.y },
				flags_ChildWindow_,
				flags_Window_
			);
		}

		static inline void End() {
			ImGui::EndChild();
			ImGui::PopID();
		}
	};

	export class TableUI {
	public:
		static void AddRectFilledAtCurrentCellTopLeftCorner(
			uint32_t rgba_,
			float width_,
			float height_
		) {
			ImGuiContext const* context{ ImGui::GetCurrentContext() };
			ImGuiWindow const* window{ context->CurrentWindow };

			auto const& style{ ImGui::GetStyle() };
			ImVec2 const min_RectFilled{
				window->DC.CursorPos.x - style.CellPadding.x,
				window->DC.CursorPos.y - style.CellPadding.y
			};
			ImVec2 const max_RectFilled{
				(width_ > 0.0f) ? (window->DC.CursorPos.x + width_) : (window->WorkRect.Max.x + style.CellPadding.x),
				window->DC.CursorPos.y + height_
			};

			ImDrawList* drawList{ ImGui::GetWindowDrawList() };
			drawList->AddRectFilled(min_RectFilled, max_RectFilled, rgba_);
		}

		static inline bool Begin(
			void const* ptr_ID_,
			int32_t num_Columns_,
			Vec2 const& size_Outer_ = Vec2{},
			float width_Inner_ = 0.0f,
			ImGuiTableFlags flags_Table_ = ImGuiTableFlags_None
		) {
			ImGui::PushID(ptr_ID_);
			return ImGui::BeginTable(
				"TableUI",
				num_Columns_,
				flags_Table_,
				ImVec2{ size_Outer_.x, size_Outer_.y },
				width_Inner_
			);
		}

		static inline void End() {
			ImGui::EndTable();
			ImGui::PopID();
		}
	};

	export template<bool IsHierarchical = true>
	class TreeNodeUI {
	public:
		static inline bool Begin(
			void const* ptr_ID_,
			char const* label_,
			ImGuiTreeNodeFlags flags_TreeNode_ = ImGuiTreeNodeFlags_None
		) {
			if constexpr (IsHierarchical) {
				flags_TreeNode_ &= ~ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			else {
				flags_TreeNode_ |= ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}

			ImGui::PushID(ptr_ID_);
			if (ImGui::TreeNodeEx(label_, flags_TreeNode_)) {
				return true;
			}
			else {
				ImGui::PopID();
				return false;
			}
		}

		static inline void End() {
			if constexpr (IsHierarchical) {
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	};

	export class ComboUI {
	public:
		static inline bool Begin(
			void const* ptr_ID_,
			std::string_view preview_,
			ImGuiComboFlags flags_Combo_ = ImGuiComboFlags_None
		) {
			ImGui::PushID(ptr_ID_);
			if(
				ImGui::BeginCombo(
					"",
					preview_.data(),
					flags_Combo_
				)
			) {
				return true;
			}
			else {
				ImGui::PopID();
				return false;
			}
		}

		template<typename T>
		static inline bool List(
			T& content_,
			int& idx_Item_,
			Lexicon<T> const& lexicon_
		) {
			bool ret{ false };
			for (int idx{ 0 }; idx < static_cast<int>(lexicon_.Size()); ++idx) {
				bool const isSelected{ (idx_Item_ == idx) };
				ImGui::PushID(idx);
				ImGui::SetNextItemAllowOverlap();
				if (ImGui::Selectable(lexicon_.Lexis(idx).data(), isSelected)) {
					if (idx_Item_ != idx) { ret = true; }
					idx_Item_ = idx;
					content_ = lexicon_.Content(idx);
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
				ImGui::PopID();
			}
			return ret;
		}

		static inline void End() {
			ImGui::EndCombo();
			ImGui::PopID();
		}
	};
}