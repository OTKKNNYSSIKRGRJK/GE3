#include "AIAssistantPanel.h"
#include <imgui.h>

namespace Editor
{
    AIAssistantPanel::AIAssistantPanel(std::shared_ptr<IAIAssistant> assistant)
        : PanelBase("AI Assistant"), m_assistant(std::move(assistant))
    {
        m_history.push_back({ ChatRole::System,
            u8"パラメータ調整のご相談や実装アイデアのご提案などお気軽にどうぞ。" });
    }

    void AIAssistantPanel::OnImGuiRender()
    {
        ImGui::Begin(GetTitle(), &IsOpenRef());

        DrawSuggestions();
        ImGui::Separator();
        DrawMessageHistory();
        DrawInputBar();

        ImGui::End();
    }

    void AIAssistantPanel::DrawSuggestions()
    {
        if (m_pendingSuggestions.empty())
        {
            return;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"AIからの提案");

        for (size_t i = 0; i < m_pendingSuggestions.size();)
        {
            const ParameterSuggestion& s = m_pendingSuggestions[i];
            ImGui::PushID(static_cast<int>(i));

            ImGui::BulletText("%s : %s -> %s",
                s.paramPath.c_str(), s.currentValue.c_str(), s.suggestedValue.c_str());
            if (!s.reason.empty())
            {
                ImGui::TextWrapped(u8"理由: %s", s.reason.c_str());
            }

            bool applied = false;
            bool dismissed = false;
            if (ImGui::SmallButton(u8"適用")) applied = true;
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"却下")) dismissed = true;

            ImGui::PopID();

            if (applied)
            {
                if (m_onApplySuggestion)
                {
                    m_onApplySuggestion(s);
                }
                m_pendingSuggestions.erase(m_pendingSuggestions.begin() + i);
            }
            else if (dismissed)
            {
                m_pendingSuggestions.erase(m_pendingSuggestions.begin() + i);
            }
            else
            {
                ++i;
            }
        }
    }

    void AIAssistantPanel::DrawMessageHistory()
    {
        ImGui::BeginChild("ChatHistory", ImVec2(0, -70), true);

        {
            std::lock_guard<std::mutex> lock(m_historyMutex);
            for (const auto& msg : m_history)
            {
                const char* label =
                    msg.role == ChatRole::User ? u8"あなた" :
                    msg.role == ChatRole::Assistant ? "AI" : "System";

                ImVec4 color =
                    msg.role == ChatRole::User ? ImVec4(0.6f, 0.8f, 1.0f, 1.0f) :
                    msg.role == ChatRole::Assistant ? ImVec4(0.8f, 1.0f, 0.7f, 1.0f) :
                                                       ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

                ImGui::TextColored(color, "%s", label);
                ImGui::TextWrapped("%s", msg.text.c_str());
                ImGui::Spacing();
            }
        }

        if (m_waitingForResponse.load())
        {
            ImGui::TextDisabled(u8"AIが応答を生成中ですわ...");
        }

        if (m_scrollToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scrollToBottom = false;
        }

        ImGui::EndChild();
    }

    void AIAssistantPanel::DrawInputBar()
    {
        ImGui::SetNextItemWidth(-80);
        ImGui::InputTextMultiline("##AIInput", m_inputBuf, sizeof(m_inputBuf), ImVec2(-80, 55));

        ImGui::SameLine();
        ImGui::BeginDisabled(m_waitingForResponse.load());
        bool sendPressed = ImGui::Button(u8"送信", ImVec2(70, 55));
        ImGui::EndDisabled();

        if (sendPressed && m_inputBuf[0] != '\0')
        {
            SendCurrentInput();
        }
    }

    void AIAssistantPanel::SendCurrentInput()
    {
        std::string userText = m_inputBuf;
        m_inputBuf[0] = '\0';

        if (!m_contextHint.empty())
        {
            userText += "\n\n[Context] " + m_contextHint;
        }

        std::vector<ChatMessage> historySnapshot;
        {
            std::lock_guard<std::mutex> lock(m_historyMutex);
            m_history.push_back({ ChatRole::User, userText });
            historySnapshot = m_history;
        }
        m_scrollToBottom = true;
        m_waitingForResponse = true;

        // SendMessageAsyncの実装（貴女様の既存API通信クラス）が
        // 別スレッドからコールバックを呼んでも安全なよう、mutexで保護しておりますわ。
        m_assistant->SendMessageAsync(
            historySnapshot,
            [this](std::string responseText)
            {
                std::lock_guard<std::mutex> lock(m_historyMutex);
                m_history.push_back({ ChatRole::Assistant, std::move(responseText) });
                m_waitingForResponse = false;
                m_scrollToBottom = true;
            },
            [this](std::string errorText)
            {
                std::lock_guard<std::mutex> lock(m_historyMutex);
                m_history.push_back({ ChatRole::System, "エラー: " + errorText });
                m_waitingForResponse = false;
                m_scrollToBottom = true;
            });
    }

    void AIAssistantPanel::PushParameterSuggestion(ParameterSuggestion suggestion)
    {
        m_pendingSuggestions.push_back(std::move(suggestion));
    }

    void AIAssistantPanel::SetOnApplySuggestion(std::function<void(const ParameterSuggestion&)> callback)
    {
        m_onApplySuggestion = std::move(callback);
    }
}
