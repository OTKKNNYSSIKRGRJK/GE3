export module Lumina.EditorTest : Panel.Inspector;

import <cstdint>;
import <functional>;

import : Panel.Base;

namespace Lumina::Editor {
	// コンポーネントの具体的な型を知らなくて済むよう、描画処理は外部から注入いたしますわ。
	// 例）SetDrawCallback([this](uint64_t id){ DrawTransformUI(id); DrawMaterialUI(id); });
	// 拡張する場合は、コンポーネント型ごとにDrawer関数を登録できる仕組みにするのがお勧めですの。
	export class InspectorPanel : public PanelBase {
	public:
		InspectorPanel();

		void OnImGuiRender() override;

		// Hierarchy等で選択が変わるたびに呼んでくださいまし。0は「未選択」の意ですわ。
		void SetSelectedId(uint64_t id) { m_selectedId = id; }
		uint64_t GetSelectedId() const { return m_selectedId; }

		void SetDrawCallback(std::function<void(uint64_t)> callback) { m_drawCallback = std::move(callback); }

	private:
		uint64_t m_selectedId = 0;
		std::function<void(uint64_t)> m_drawCallback;
	};
}
