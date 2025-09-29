module Lumina.Editor.DX12 : RootSignature;

namespace {
	template<typename T>
	using UniPtr = std::unique_ptr<T>;
}

namespace Lumina::Editor {
	void Module<DX12RootSignature>::Update() {
		if (ImGui::BeginTabItem("Root Signature")) {
			if (
				PanelUI::Begin(
					"Root Signature",
					Vec2{},
					ImGuiChildFlags_Borders,
					ImGuiWindowFlags_NoCollapse
				)
			) {
				if (
					ImGui::BeginTabBar(
						"Root Signature",
						ImGuiTabBarFlags_FittingPolicyScroll |
						ImGuiTabBarFlags_TabListPopupButton
					)
				) {
					if (ImGui::BeginTabItem("Root Parameters")) {
						Module_RootParameterList_.Update();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Root Descriptor Tables")) {
						Module_RootTableList_.Update();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}

				PanelUI::End();
			}

			ImGui::EndTabItem();
		}
	}

	void Module<DX12RootSignature>::Initialize() {
		Module_RootParameterList_.Initialize();
		Module_RootTableList_.Initialize();
	}

	auto Module<DX12RootSignature>::Create() -> UniPtr<SelfType> {
		return UniPtr<SelfType>{ new SelfType{} };
	}
}