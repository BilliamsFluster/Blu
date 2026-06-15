#pragma once
#include <cstdint>

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
}
