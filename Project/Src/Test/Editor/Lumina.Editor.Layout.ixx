export module Lumina.EditorTest : Layout;

import <memory>;
import <vector>;

import : Panel.Base;

namespace Lumina::Editor {
	// DockSpaceの構築、初期レイアウトの固定、登録済みパネルの一括描画を担当いたしますわ。
	export class EditorLayout {
	public:
		void RegisterPanel(std::unique_ptr<IEditorPanel> panel);

		// 毎フレーム呼び出してくださいまし。DockSpaceの構築＆全パネル描画をいたします。
		void OnImGuiRender();

		// メインメニューバーの「Window」項目から呼ぶと、パネルの表示切替メニューが出ますわ。
		// 例）if (ImGui::BeginMenuBar()) { layout.DrawWindowMenu(); ImGui::EndMenuBar(); }
		void DrawWindowMenu();

	private:
		void SetupDockspaceHost();
		void BuildInitialLayout(unsigned int dockspaceId);

		std::vector<std::unique_ptr<IEditorPanel>> m_panels;
		bool m_dockLayoutInitialized = false;
	};
}
