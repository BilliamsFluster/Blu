#pragma once

#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>

#include <thread>
namespace Blu
{


    struct ProfileResult
    {
        std::string Name;
        long long Start, End;
        uint32_t ThreadID;
    };

    struct InstrumentationSession
    {
        std::string Name;
    };

    class Instrumentor
    {
    private:
        InstrumentationSession* m_CurrentSession;
        std::ofstream m_OutputStream;
        int m_ProfileCount;
    public:
        Instrumentor()
            : m_CurrentSession(nullptr), m_ProfileCount(0)
        {
        }

        void BeginSession(const std::string& name, const std::string& filepath = "results.json")
        {
            // A UI-driven trace can begin while the ambient session is live; close it first so
            // the stream isn't already-open (ofstream::open on an open stream silently fails).
            if (m_CurrentSession)
                EndSession();
            m_OutputStream.open(filepath);
            WriteHeader();
            m_CurrentSession = new InstrumentationSession{ name };
        }

        void EndSession()
        {
            if (!m_CurrentSession)
                return; // null-safe: tolerate an extra EndSession (e.g. after a UI-stopped trace)
            WriteFooter();
            m_OutputStream.close();
            delete m_CurrentSession;
            m_CurrentSession = nullptr;
            m_ProfileCount = 0;
        }

        bool IsSessionActive() const { return m_CurrentSession != nullptr; }

        void WriteProfile(const ProfileResult& result)
        {
            if (m_ProfileCount++ > 0)
                m_OutputStream << ",";

            std::string name = result.Name;
            std::replace(name.begin(), name.end(), '"', '\'');

            m_OutputStream << "{";
            m_OutputStream << "\"cat\":\"function\",";
            m_OutputStream << "\"dur\":" << (result.End - result.Start) << ',';
            m_OutputStream << "\"name\":\"" << name << "\",";
            m_OutputStream << "\"ph\":\"X\",";
            m_OutputStream << "\"pid\":0,";
            m_OutputStream << "\"tid\":" << result.ThreadID << ",";
            m_OutputStream << "\"ts\":" << result.Start;
            m_OutputStream << "}";

            m_OutputStream.flush();
        }

        void WriteHeader()
        {
            m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
            m_OutputStream.flush();
        }

        void WriteFooter()
        {
            m_OutputStream << "]}";
            m_OutputStream.flush();
        }

        static Instrumentor& Get()
        {
            static Instrumentor instance;
            return instance;
        }
    };

    class InstrumentationTimer
    {
    public:
        InstrumentationTimer(const char* name)
            : m_Name(name), m_Stopped(false)
        {
            m_StartTimepoint = std::chrono::high_resolution_clock::now();
        }

        ~InstrumentationTimer()
        {
            if (!m_Stopped)
                Stop();
        }

        void Stop()
        {
            auto endTimepoint = std::chrono::high_resolution_clock::now();

            long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch().count();
            long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

            uint32_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
            Instrumentor::Get().WriteProfile({ m_Name, start, end, threadID });

            m_Stopped = true;
        }
    private:
        const char* m_Name;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
        bool m_Stopped;
    };
}
// ── Profiler backend selection ───────────────────────────────────────────────
// Default: the built-in chrome://tracing instrumentor above. Define BLU_USE_TRACY (and vendor
// Tracy under ExternalDependencies/tracy + add its include dir and TracyClient.cpp to the Blu
// project in premake5.lua) to route the SAME BLU_PROFILE_* macros to Tracy's live sampling
// profiler instead — no call sites change. BLU_FRAME_MARK() delimits frames for the profiler UI.
#if defined(BLU_USE_TRACY)
    #include <tracy/Tracy.hpp>
    #define BLU_PROFILE_BEGIN_SESSION(name, filepath)
    #define BLU_PROFILE_END_SESSION()
    #define BLU_PROFILE_SCOPE(name) ZoneScopedN(name)
    #define BLU_PROFILE_FUNCTION() ZoneScoped
    #define BLU_FRAME_MARK() FrameMark
#else
    #define BLU_PROFILE 1
    #if BLU_PROFILE
    #define BLU_PROFILE_BEGIN_SESSION(name, filepath) ::Blu::Instrumentor::Get().BeginSession(name, filepath)
    #define BLU_PROFILE_END_SESSION() ::Blu::Instrumentor::Get().EndSession()
    #define BLU_PROFILE_SCOPE(name) ::Blu::InstrumentationTimer timer##__LINE__(name);
    #define BLU_PROFILE_FUNCTION() BLU_PROFILE_SCOPE(__FUNCSIG__)
    #else
    #define BLU_PROFILE_BEGIN_SESSION(name, filepath)
    #define BLU_PROFILE_END_SESSION()
    #define BLU_PROFILE_SCOPE(name)
    #define BLU_PROFILE_FUNCTION()
    #endif
    #define BLU_FRAME_MARK()
#endif