#pragma once
// EditorLog — captures engine log messages and exposes them to the Output Log panel.
//
// Usage:
//   1. At startup, call EditorLog::RegisterSinks() to attach to both core and client loggers.
//   2. In OnGuiDraw, iterate EditorLog::Get().GetMessages() inside a child window.

#include <string>
#include <deque>
#include <mutex>
#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>
#include "Blu/Core/Log.h"

namespace Blu
{
    // -----------------------------------------------------------------------
    enum class EditorLogLevel { Trace = 0, Info, Warn, Error };

    struct LogEntry
    {
        EditorLogLevel Level = EditorLogLevel::Info;
        std::string    Text;
    };

    // -----------------------------------------------------------------------
    // Singleton message store — safe to write from any thread, read from ImGui
    // render thread only.
    // -----------------------------------------------------------------------
    class EditorLog
    {
    public:
        static EditorLog& Get()
        {
            static EditorLog s_Instance;
            return s_Instance;
        }

        void AddMessage(EditorLogLevel level, std::string text)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Messages.push_back({ level, std::move(text) });
            if (m_Messages.size() > kMaxMessages)
                m_Messages.pop_front();
            m_ScrollToBottom = true;
        }

        // Call from the ImGui render thread only.
        const std::deque<LogEntry>& GetMessages() const { return m_Messages; }

        bool ConsumeScrollToBottom()
        {
            bool s = m_ScrollToBottom;
            m_ScrollToBottom = false;
            return s;
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Messages.clear();
        }

        // Attach to both loggers. Call once during OnAttach.
        static void RegisterSinks();

    private:
        EditorLog() = default;

        static constexpr size_t kMaxMessages = 500;
        std::deque<LogEntry> m_Messages;
        std::mutex           m_Mutex;
        bool                 m_ScrollToBottom = false;
    };

    // -----------------------------------------------------------------------
    // spdlog sink template — forwards formatted lines to EditorLog.
    // -----------------------------------------------------------------------
    template<typename Mutex>
    class EditorLogSink final : public spdlog::sinks::base_sink<Mutex>
    {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            spdlog::memory_buf_t buf;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, buf);
            // spdlog::details::to_string_view works for both std::string and
            // fmt::basic_memory_buffer (depending on SPDLOG_USE_STD_FORMAT).
            auto sv = spdlog::details::to_string_view(buf);
            std::string text(sv.data(), sv.size());
            // Strip trailing newline that spdlog appends.
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.pop_back();

            EditorLogLevel level = EditorLogLevel::Info;
            if      (msg.level == spdlog::level::trace) level = EditorLogLevel::Trace;
            else if (msg.level == spdlog::level::warn)  level = EditorLogLevel::Warn;
            else if (msg.level >= spdlog::level::err)   level = EditorLogLevel::Error;

            EditorLog::Get().AddMessage(level, std::move(text));
        }

        void flush_() override {}
    };

    using EditorLogSink_mt = EditorLogSink<std::mutex>;

    // -----------------------------------------------------------------------
    inline void EditorLog::RegisterSinks()
    {
        auto sink = std::make_shared<EditorLogSink_mt>();
        Log::GetCoreLogger()->sinks().push_back(sink);
        Log::GetClientLogger()->sinks().push_back(sink);
    }
}
