#pragma once
#include "../PanelBase.h"
#include <imgui.h>

namespace Editor
{
    // シーンをオフスクリーンレンダリングした結果をImGui::Imageで表示するだけの
    // 薄いラッパーですわ。D3D12側のSRV記述子ハンドル(D3D12_GPU_DESCRIPTOR_HANDLE.ptr)を
    // ImTextureIDへreinterpret_castして渡してくださいまし。
    //
    // TODO: ギズモ操作を追加する場合はImGuizmo::SetRect / Manipulateをここに差し込むと
    //       ちょうどこのウインドウの矩形内で完結いたしますわ。
    class ScenePanel : public PanelBase
    {
    public:
        ScenePanel();

        void OnImGuiRender() override;

        void SetSceneTexture(ImTextureID textureId, float texWidth, float texHeight);

        struct ViewportSize
        {
            float width = 0.0f;
            float height = 0.0f;
        };

        // フレーム末尾で確認し、前回と変わっていたらオフスクリーンRTV/SRVの
        // 再生成（リサイズ）をトリガーしてくださいまし。
        const ViewportSize& GetViewportSize() const { return m_viewportSize; }

        bool IsFocused() const { return m_isFocused; }
        bool IsHovered() const { return m_isHovered; }

    private:
        ImTextureID m_sceneTexture = nullptr;
        float m_texWidth = 0.0f;
        float m_texHeight = 0.0f;
        ViewportSize m_viewportSize;
        bool m_isFocused = false;
        bool m_isHovered = false;
    };
}
