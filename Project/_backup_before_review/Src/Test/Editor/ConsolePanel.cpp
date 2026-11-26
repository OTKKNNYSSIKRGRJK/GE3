#include "ConsolePanel.h"
#include <imgui.h>

namespace Editor
{
    namespace
    {
        ImVec4 ColorForLevel(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Warning: return ImVec4(0.95f, 0.75f, 0.2f, 1.0f);
                case LogLevel::Error:   return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                default:                return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            }
        }

        const char* PrefixForLevel(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Warning: return "[WARN] ";
                case LogLevel::Error:   return "[ERROR] ";
                default:                return "[INFO] ";
            }
        }
    }

    ConsolePanel::ConsolePanel() : PanelBase("Console")
    {
    }

    void ConsolePanel::AddLog(LogLevel level, const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back({ level, message });
    }

    void ConsolePanel::OnImGuiRender()
    {
        ImGui::Begin(GetTitle(), &IsOpenRef());

        if (ImGui::Button(u8"クリア"))
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_entries.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox(u8"自動スクロール", &m_autoScroll);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##Filter", u8"フィルタ...", m_filterBuf, sizeof(m_filterBuf));

        ImGui::Separator();
        ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_entries)
        {
            if (m_filterBuf[0] != '\0' && entry.text.find(m_filterBuf) == std::string::npos)
            {
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, ColorForLevel(entry.level));
            ImGui::TextUnformatted(PrefixForLevel(entry.level));
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextWrapped("%s", entry.text.c_str());
            ImGui::PopStyleColor();
        }

        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }
}
