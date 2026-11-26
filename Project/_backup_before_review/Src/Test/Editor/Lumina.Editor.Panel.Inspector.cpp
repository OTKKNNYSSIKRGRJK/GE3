module;

#include <imgui.h>

module Lumina.EditorTest;

import : Panel.Inspector;

namespace Lumina::Editor {
	InspectorPanel::InspectorPanel() : PanelBase("Inspector") {}

	void InspectorPanel::OnImGuiRender() {
		ImGui::Begin(GetTitle(), &IsOpenRef());

		if (m_selectedId == 0) {
			ImGui::TextDisabled("No object selected");
		}
		else if (m_drawCallback) {
			m_drawCallback(m_selectedId);
		}
		else {
			ImGui::Text("Selected: %llu", static_cast<unsigned long long>(m_selectedId));
			ImGui::TextDisabled("SetDrawCallback is not set");
		}

		ImGui::End();
	}
}
