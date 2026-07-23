#pragma once
#include "../PanelBase.h"
#include <mutex>
#include <string>
#include <vector>

namespace Editor
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error
    };

    class ConsolePanel : public PanelBase
    {
    public:
        ConsolePanel();

        void OnImGuiRender() override;

        // エンジンの他部分（レンダラ・Python連携・アセットローダ等）から
        // 好きなスレッドで呼んでいただいて構いませんわ。
        void AddLog(LogLevel level, const std::string& message);

    private:
        struct LogEntry
        {
            LogLevel level;
            std::string text;
        };

        std::vector<LogEntry> m_entries;
        std::mutex m_mutex;
        bool m_autoScroll = true;
        char m_filterBuf[128] = {};
    };
}
