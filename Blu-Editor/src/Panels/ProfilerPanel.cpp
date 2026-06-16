#include "ProfilerPanel.h"
#include "imgui.h"
#include "Blu/Debug/PerfStats.h"
#include "Blu/Core/JobSystem.h"
#include "Blu/Rendering/AssetManager.h"

namespace Blu
{
	void ProfilerPanel::OnImGuiRender(bool* open)
	{
		if (open && !*open)
			return;
		if (!ImGui::Begin("Profiler", open))
		{
			ImGui::End();
			return;
		}

		// ── CPU subsystem breakdown (previous frame) ───────────────────────────
		auto& prof = Perf::FrameProfiler::Get();
		ImGui::Text("CPU (measured subsystems): %.2f ms", prof.LastFrameTotalMs());
		if (ImGui::BeginTable("ProfilerCpu", 2,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Subsystem");
			ImGui::TableSetupColumn("ms");
			ImGui::TableHeadersRow();
			for (const Perf::CpuSample& sample : prof.LastFrame())
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(sample.Name.c_str());
				ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", sample.Milliseconds);
			}
			ImGui::EndTable();
		}

		ImGui::Separator();

		// ── Job system ─────────────────────────────────────────────────────────
		JobSystem& jobs = JobSystem::Get();
		ImGui::Text("Job System: %d worker(s)", jobs.WorkerCount());
		ImGui::Text("  queued tasks: %zu   main-thread queue: %zu",
			jobs.PendingTaskCount(), jobs.MainThreadQueueDepth());

		ImGui::Separator();

		// ── Asset cache (LRU + memory budget) ───────────────────────────────────
		const AssetCacheStats assets = AssetManager::Get().GetCacheStats();
		ImGui::Text("Assets resident: %zu  (referenced: %zu)", assets.ResidentCount, assets.ReferencedCount);
		if (assets.BudgetBytes != 0)
			ImGui::Text("Asset memory: %.2f / %.2f MiB",
				Perf::BytesToMiB(assets.ResidentBytes), Perf::BytesToMiB(assets.BudgetBytes));
		else
			ImGui::Text("Asset memory: %.2f MiB (budget: unlimited)", Perf::BytesToMiB(assets.ResidentBytes));
		ImGui::Text("Evictions (since start): %zu", assets.Evictions);

		ImGui::Separator();

		// ── Process memory ───────────────────────────────────────────────────────
		const Perf::MemoryInfo mem = Perf::QueryProcessMemory();
		ImGui::Text("Process working set: %.1f MiB (peak %.1f)",
			Perf::BytesToMiB(mem.WorkingSetBytes), Perf::BytesToMiB(mem.PeakWorkingSetBytes));
		ImGui::Text("Private bytes: %.1f MiB", Perf::BytesToMiB(mem.PrivateBytes));

		ImGui::End();
	}
}
