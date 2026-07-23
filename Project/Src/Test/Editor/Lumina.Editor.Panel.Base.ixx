export module Lumina.EditorTest : Panel.Base;

import <string>;
import <memory>;

namespace Lumina::Editor {
	// すべてのエディタウインドウが実装するインターフェースですわ。
	// EditorLayoutがこれを介してまとめて管理・描画いたします。
	class IEditorPanel {
	public:
		virtual ~IEditorPanel() = default;

		// ImGui::Begin/Endを含む描画処理。毎フレーム呼ばれますの。
		virtual void OnImGuiRender() = 0;

		// ウインドウタイトル。DockBuilderのDockWindow指定にも使いますので、
		// 各パネルで一意な文字列にしてくださいまし。
		virtual const char* GetTitle() const = 0;

		// 表示/非表示フラグへの参照。「Window」メニューからのトグルに使いますわ。
		virtual bool& IsOpenRef() = 0;
	};
}

namespace Lumina::Editor {
	// タイトルとOpenフラグの管理だけを肩代わりする基底クラスですわ。
	// 各パネルはこれを継承してOnImGuiRenderだけ実装すればよろしくてよ。
	class PanelBase : public IEditorPanel {
	public:
		explicit PanelBase(std::string title, bool openByDefault = true)
			: m_title(std::move(title)), m_isOpen(openByDefault)
		{}

		const char* GetTitle() const override { return m_title.c_str(); }
		bool& IsOpenRef() override { return m_isOpen; }

	protected:
		std::string m_title;
		bool m_isOpen;
	};
}
