module Game.DebugDock;

//****	******	******	******	******	****//

// ここで import しているヘッダユニットは、いずれも Game.DebugDock.ixx で import 済み
import <array>;
import <cstddef>;
import <cstdint>;
import <string_view>;

#if defined(_DEBUG)
import Lumina.Utils.ImGui;
#endif

//////	//////	//////	//////	//////	//////

namespace Game {
	void DebugDock::Begin([[maybe_unused]] bool toggleHeld_) {
#if defined(_DEBUG)
		if (toggleHeld_ && !PrevToggle_) { AllHidden_ = !AllHidden_; }
		PrevToggle_ = toggleHeld_;
		if (AllHidden_) { return; }

		// メニュー（画面の上）。ここで作ると、メインのビューポートの WorkPos がメニューの下へずれる（ドックはその下に入る）
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("Editors")) {
				Area area{ Area::Upper };
				ImGui::SeparatorText("Right / upper");
				for (Entry& e : Entries_) {
					if (e.Title.empty()) { continue; }		// 配列の余り（名前なし）。data() が nullptr なので ImGui に渡さない
					if (e.Where != area) {
						area = e.Where;
						ImGui::SeparatorText("Right / lower");
					}
					ImGui::MenuItem(e.Title.data(), nullptr, &e.Visible);
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Reset layout")) { ResetLayout_ = true; }
				if (ImGui::MenuItem("Hide all", "F2")) { AllHidden_ = true; }
				ImGui::EndMenu();
			}
			ImGui::TextDisabled("F1: cursor  F2: hide editors");
			ImGui::EndMainMenuBar();
		}
		if (AllHidden_) { return; }

		// 右の列（上下 2 段）のドック。真ん中は透かして、ドッキングもさせない
		ImGuiViewport const* const viewport{ ImGui::GetMainViewport() };
		ImGuiID const dockId{ ImHashStr("Game.DebugDock") };
		ImGuiDockNode const* const node{ ImGui::DockBuilderGetNode(dockId) };
		if (ResetLayout_ || node == nullptr || node->IsLeafNode()) {
			ResetLayout_ = false;
			ImGui::DockBuilderRemoveNode(dockId);
			ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodePos(dockId, viewport->WorkPos);
			ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);
			ImGuiID center{ dockId };
			ImGuiID upper{ ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.36f, nullptr, &center) };
			ImGuiID const lower{ ImGui::DockBuilderSplitNode(upper, ImGuiDir_Down, 0.55f, nullptr, &upper) };
			for (Entry const& e : Entries_) {
				if (e.Title.empty()) { continue; }
				ImGui::DockBuilderDockWindow(e.Title.data(), (e.Where == Area::Upper) ? upper : lower);
			}
			ImGui::DockBuilderFinish(dockId);
		}
		ImGui::DockSpaceOverViewport(
			dockId,
			viewport,
			ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode
		);
#endif
	}

	auto DebugDock::Visible(std::string_view title_) const noexcept -> bool {
		if (AllHidden_) { return false; }
		for (Entry const& e : Entries_) {
			if (e.Title == title_) { return e.Visible; }
		}
		return true;
	}
}
