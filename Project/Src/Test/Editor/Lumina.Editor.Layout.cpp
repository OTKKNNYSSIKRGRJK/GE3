module;

#include <imgui.h>
#include <imgui_internal.h> // DockBuilder系APIを使うために必要ですわ

module Lumina.EditorTest;

import : Layout;

namespace Lumina::Editor {
	void EditorLayout::RegisterPanel(std::unique_ptr<IEditorPanel> panel) {
		m_panels.push_back(std::move(panel));
	}

	void EditorLayout::OnImGuiRender() {
		SetupDockspaceHost();

		for (auto& panel : m_panels)
		{
			if (panel->IsOpenRef())
			{
				panel->OnImGuiRender();
			}
		}
	}

	void EditorLayout::DrawWindowMenu() {
		if (ImGui::BeginMenu("Window"))
		{
			for (auto& panel : m_panels)
			{
				ImGui::MenuItem(panel->GetTitle(), nullptr, &panel->IsOpenRef());
			}
			ImGui::EndMenu();
		}
	}

	void EditorLayout::SetupDockspaceHost() {
		const ImGuiWindowFlags hostFlags =
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("EditorDockSpaceHost", nullptr, hostFlags);
		ImGui::PopStyleVar(3);

		const ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");
		ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		if (!m_dockLayoutInitialized)
		{
			BuildInitialLayout(dockspaceId);
			m_dockLayoutInitialized = true;
		}

		ImGui::End();
	}

	void EditorLayout::BuildInitialLayout(unsigned int dockspaceIdRaw)
	{
		const ImGuiID dockspaceId = dockspaceIdRaw;

		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

		// Unity風レイアウト：
		//   左   = Hierarchy
		//   中央 = Scene（ビューポート）
		//   右上 = Inspector / 右下 = AI Assistant
		//   下   = Console
		ImGuiID dockMain = dockspaceId;
		ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, nullptr, &dockMain);
		ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.28f, nullptr, &dockMain);
		ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, nullptr, &dockMain);
		ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.50f, nullptr, &dockRight);

		// ここで指定する文字列は各パネルのGetTitle()と完全一致させてくださいまし。
		ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
		ImGui::DockBuilderDockWindow("Scene", dockMain);
		ImGui::DockBuilderDockWindow("Inspector", dockRight);
		ImGui::DockBuilderDockWindow("AI Assistant", dockRightBottom);
		ImGui::DockBuilderDockWindow("Console", dockBottom);

		ImGui::DockBuilderFinish(dockspaceId);
	}
}
