#pragma once

namespace Editor
{
    // すべてのエディタウインドウが実装するインターフェースですわ。
    // EditorLayoutがこれを介してまとめて管理・描画いたします。
    class IEditorPanel
    {
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
