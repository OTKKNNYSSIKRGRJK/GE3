#pragma once
#include "../PanelBase.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Editor
{
    enum class ChatRole
    {
        User,
        Assistant,
        System
    };

    struct ChatMessage
    {
        ChatRole role;
        std::string text;
    };

    // AIが「このパラメータをこの値に」と提案してきた内容を表しますわ。
    // AI応答（JSON等）のパースはエンジン側の実装にお任せし、結果をこの構造体に
    // 詰めてPushParameterSuggestionへ渡していただく設計ですの。
    struct ParameterSuggestion
    {
        std::string paramPath;      // 例: "Light.Directional.Angle"
        std::string currentValue;
        std::string suggestedValue;
        std::string reason;         // AIがその値を勧める理由（任意）
    };

    // 既存のAI API通信部分をこのインターフェースの実装でラップしていただければ、
    // AIAssistantPanel側は通信方式（クラウドAPI／将来のローカルLLM）を一切気にせずに済みますわ。
    class IAIAssistant
    {
    public:
        virtual ~IAIAssistant() = default;

        // 非同期送信。ワーカースレッドからonResponse/onErrorを呼んでいただいて構いません。
        // （このパネル内部では該当データをmutexで保護しております）
        virtual void SendMessageAsync(
            const std::vector<ChatMessage>& history,
            std::function<void(std::string responseText)> onResponse,
            std::function<void(std::string errorText)> onError) = 0;
    };

    class AIAssistantPanel : public PanelBase
    {
    public:
        explicit AIAssistantPanel(std::shared_ptr<IAIAssistant> assistant);

        void OnImGuiRender() override;

        // AI応答からパラメータ提案を抽出できた場合に呼んでくださいまし。
        // パネル上部に「適用／却下」ボタン付きで表示されますわ。
        void PushParameterSuggestion(ParameterSuggestion suggestion);

        // 「適用」が押されたときに実際のパラメータへ反映する処理をここに実装してくださいまし。
        void SetOnApplySuggestion(std::function<void(const ParameterSuggestion&)> callback);

        // Scene/Inspectorで選択中の対象などをAIへの追加コンテキストとして渡す場合に使いますの。
        void SetContextHint(std::string hint) { m_contextHint = std::move(hint); }

    private:
        void DrawSuggestions();
        void DrawMessageHistory();
        void DrawInputBar();
        void SendCurrentInput();

        std::shared_ptr<IAIAssistant> m_assistant;

        std::vector<ChatMessage> m_history;
        std::mutex m_historyMutex;

        std::vector<ParameterSuggestion> m_pendingSuggestions;
        std::function<void(const ParameterSuggestion&)> m_onApplySuggestion;

        char m_inputBuf[2048] = {};
        std::atomic<bool> m_waitingForResponse{ false };
        std::string m_contextHint;
        bool m_scrollToBottom = false;
    };
}
