#pragma once
#include <cstdint>
#include <chrono>
#include <string>
#include <vector>

namespace Blu::Perf
{
	// Process-level memory usage, queried from the OS (no allocator hooks, no overhead until
	// asked). Surfaced live in the editor Diagnostics panel and usable from any tooling.
	struct MemoryInfo
	{
		uint64_t WorkingSetBytes     = 0; // physical RAM currently mapped to the process
		uint64_t PeakWorkingSetBytes = 0; // high-water mark since launch
		uint64_t PrivateBytes        = 0; // committed private bytes (commit charge)
	};

	// Returns the current process memory usage. All-zero if the platform query is unavailable.
	MemoryInfo QueryProcessMemory();

	// Convenience: bytes → mebibytes for display.
	inline double BytesToMiB(uint64_t bytes) { return (double)bytes / (1024.0 * 1024.0); }

	// ── Per-frame CPU subsystem timing ───────────────────────────────────────────
	// Lightweight named-bucket CPU profiler: scoped samples accumulate into the current frame;
	// BeginFrame() rolls the accumulation into a stable last-frame snapshot the editor reads, then
	// clears for the next frame. Single-threaded (call Add/BeginFrame from the main update thread).
	struct CpuSample
	{
		std::string Name;
		double      Milliseconds = 0.0;
	};

	class FrameProfiler
	{
	public:
		static FrameProfiler& Get();

		void Add(const std::string& name, double milliseconds); // accumulate into a named bucket
		void BeginFrame();                                       // snapshot -> last frame; clear current

		const std::vector<CpuSample>& LastFrame() const { return m_LastFrame; }
		double LastFrameTotalMs() const;

	private:
		std::vector<CpuSample> m_Current;
		std::vector<CpuSample> m_LastFrame;
	};

	// RAII scope timer: adds its wall-clock duration to a FrameProfiler bucket on destruction.
	class ScopedCpuTimer
	{
	public:
		explicit ScopedCpuTimer(const char* name)
			: m_Name(name), m_Start(std::chrono::high_resolution_clock::now()) {}
		~ScopedCpuTimer()
		{
			const auto end = std::chrono::high_resolution_clock::now();
			const double ms = std::chrono::duration<double, std::milli>(end - m_Start).count();
			FrameProfiler::Get().Add(m_Name, ms);
		}
	private:
		const char* m_Name;
		std::chrono::high_resolution_clock::time_point m_Start;
	};
}

// Times the enclosing scope into the named per-frame CPU bucket (no-op cost beyond a clock read).
#define BLU_PERF_CONCAT_(a, b) a##b
#define BLU_PERF_CONCAT(a, b) BLU_PERF_CONCAT_(a, b)
#define BLU_PERF_SCOPE(name) ::Blu::Perf::ScopedCpuTimer BLU_PERF_CONCAT(bluPerfTimer_, __LINE__)(name)
