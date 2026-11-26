export module Lumina.EditorTest : Panel.Hierarchy;

import <cstdint>;
import <functional>;
import <string>;
import <vector>;

import : Panel.Base;

namespace Lumina::Editor
{
	// TODO: idは将来的に貴女様のエンジンのHandle<EntityTag>等に置き換えていただくと
	//       世代インデックスによる安全な参照ができますわ。ここでは疎結合にするため
	//       uint64_tのまま扱っております。
	export struct HierarchyNode {
		std::string name;
		uint64_t id = 0;
		std::vector<HierarchyNode> children;
	};

	export class HierarchyPanel : public PanelBase {
	public:
		HierarchyPanel();

		void OnImGuiRender() override;

		void SetRoots(std::vector<HierarchyNode> roots) { m_roots = std::move(roots); }

		// ノードが選択された際に呼ばれますわ。InspectorPanel::SetSelectedIdと繋げてくださいまし。
		void SetOnSelect(std::function<void(uint64_t)> callback) { m_onSelect = std::move(callback); }

		uint64_t GetSelectedId() const { return m_selectedId; }

	private:
		void DrawNode(HierarchyNode& node);

		std::vector<HierarchyNode> m_roots;
		uint64_t m_selectedId = 0;
		std::function<void(uint64_t)> m_onSelect;
	};
}
