#include "Blupch.h"
#include "PerfStats.h"

#if defined(BLU_PLATFORM_WINDOWS)
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

namespace Blu::Perf
{
	MemoryInfo QueryProcessMemory()
	{
		MemoryInfo info;
#if defined(BLU_PLATFORM_WINDOWS)
		PROCESS_MEMORY_COUNTERS_EX pmc{};
		pmc.cb = sizeof(pmc);
		if (GetProcessMemoryInfo(GetCurrentProcess(),
		                         reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
		{
			info.WorkingSetBytes     = static_cast<uint64_t>(pmc.WorkingSetSize);
			info.PeakWorkingSetBytes = static_cast<uint64_t>(pmc.PeakWorkingSetSize);
			info.PrivateBytes        = static_cast<uint64_t>(pmc.PrivateUsage);
		}
#endif
		return info;
	}

	FrameProfiler& FrameProfiler::Get()
	{
		static FrameProfiler s_Instance;
		return s_Instance;
	}

	void FrameProfiler::Add(const std::string& name, double milliseconds)
	{
		for (CpuSample& sample : m_Current)
		{
			if (sample.Name == name)
			{
				sample.Milliseconds += milliseconds;
				return;
			}
		}
		m_Current.push_back({ name, milliseconds });
	}

	void FrameProfiler::BeginFrame()
	{
		m_LastFrame = m_Current; // stable snapshot the editor reads for the previous frame
		m_Current.clear();
	}

	double FrameProfiler::LastFrameTotalMs() const
	{
		double total = 0.0;
		for (const CpuSample& sample : m_LastFrame)
			total += sample.Milliseconds;
		return total;
	}
}
