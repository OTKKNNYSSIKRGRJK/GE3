module;

#include <imgui.h>

module Lumina.EditorTest;

import : Panel.Hierarchy;

namespace Lumina::Editor {
	HierarchyPanel::HierarchyPanel() : PanelBase("Hierarchy") {}

	void HierarchyPanel::OnImGuiRender() {
		ImGui::Begin(GetTitle(), &IsOpenRef());

		for (auto& root : m_roots) {
			DrawNode(root);
		}

		if (m_roots.empty()) {
			ImGui::TextDisabled("No objects in the scene");
		}

		ImGui::End();
	}

	void HierarchyPanel::DrawNode(HierarchyNode& node) {
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (node.children.empty()) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (node.id == m_selectedId) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::PushID(static_cast<int>(node.id));
		bool opened = ImGui::TreeNodeEx(node.name.c_str(), flags);

		if (ImGui::IsItemClicked()) {
			m_selectedId = node.id;
			if (m_onSelect)
			{
				m_onSelect(node.id);
			}
		}

		if (opened && !node.children.empty()) {
			for (auto& child : node.children)
			{
				DrawNode(child);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}
}
