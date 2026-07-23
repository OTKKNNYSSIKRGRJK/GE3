#include "ScenePanel.h"

namespace Editor
{
    ScenePanel::ScenePanel() : PanelBase("Scene")
    {
    }

    void ScenePanel::SetSceneTexture(ImTextureID textureId, float texWidth, float texHeight)
    {
        m_sceneTexture = textureId;
        m_texWidth = texWidth;
        m_texHeight = texHeight;
    }

    void ScenePanel::OnImGuiRender()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin(GetTitle(), &IsOpenRef(), ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        m_viewportSize.width = avail.x;
        m_viewportSize.height = avail.y;
        m_isFocused = ImGui::IsWindowFocused();
        m_isHovered = ImGui::IsWindowHovered();

        if (m_sceneTexture != nullptr && avail.x > 0.0f && avail.y > 0.0f)
        {
            // アスペクト比を保ってレターボックス表示にしていますわ。
            // 単純に引き伸ばして構わない場合はImGui::Image(m_sceneTexture, avail)だけで十分ですの。
            float texAspect = m_texWidth / m_texHeight;
            float availAspect = avail.x / avail.y;

            ImVec2 drawSize = avail;
            if (texAspect > availAspect)
            {
                drawSize.y = avail.x / texAspect;
            }
            else
            {
                drawSize.x = avail.y * texAspect;
            }

            ImVec2 cursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(
                cursor.x + (avail.x - drawSize.x) * 0.5f,
                cursor.y + (avail.y - drawSize.y) * 0.5f));

            ImGui::Image(m_sceneTexture, drawSize);
        }
        else
        {
            ImGui::TextDisabled(u8"シーンテクスチャが未設定ですわ（SetSceneTextureを呼んでくださいまし）");
        }

        ImGui::End();
    }
}
