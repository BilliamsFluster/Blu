#pragma once

namespace Blu
{
	// Editor diagnostics panel: per-subsystem CPU frame timings (Perf::FrameProfiler), job-system
	// occupancy, the asset cache budget/residency/evictions, and process memory. The verification
	// surface for the engine performance work (job system, asset LRU, draw-call batching).
	class ProfilerPanel
	{
	public:
		void OnImGuiRender(bool* open);
	};
}
