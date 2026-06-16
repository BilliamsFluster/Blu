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
}
