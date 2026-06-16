#include "Blupch.h"
#include "JobSystem.h"
#include "FrameArena.h"
#include "Blu/Core/Log.h"
#include "Blu/Debug/Instrumentor.h"

#include <algorithm>

namespace Blu
{
	// Lane of the calling thread: 0 = main (set in Initialize), 1..N = worker (set in WorkerLoop),
	// -1 = a thread the job system doesn't own (e.g. Jolt/miniaudio). Used for OnWorkerThread()
	// and per-lane scratch arenas.
	static thread_local int t_Lane = -1;

	static constexpr size_t kWorkerArenaBytes = 1u << 20; // 1 MiB scratch per lane

	JobSystem& JobSystem::Get()
	{
		static JobSystem s_Instance;
		return s_Instance;
	}

	JobSystem::~JobSystem()
	{
		Shutdown();
	}

	void JobSystem::Initialize(int workerCount)
	{
		if (m_Initialized)
			return;

		// The calling (main) thread is lane 0.
		t_Lane = 0;

		int hw = (int)std::thread::hardware_concurrency();
		if (hw <= 0) hw = 4;
		int workers = (workerCount < 0) ? (hw - 1) : workerCount;
		workers = std::clamp(workers, 0, 64);

		// One scratch arena per lane: index 0 = main, 1..workers = worker threads.
		m_Arenas.clear();
		m_Arenas.reserve((size_t)workers + 1);
		for (int i = 0; i <= workers; ++i)
			m_Arenas.push_back(std::make_unique<FrameArena>(kWorkerArenaBytes));

		m_Running.store(true, std::memory_order_release);
		m_Workers.reserve((size_t)workers);
		for (int i = 0; i < workers; ++i)
			m_Workers.emplace_back([this, lane = i + 1]() { WorkerLoop(lane); });

		m_Initialized = true;
		BLU_CORE_INFO("JobSystem initialized with {0} worker thread(s)", workers);
	}

	void JobSystem::Shutdown()
	{
		if (!m_Initialized)
			return;

		m_Running.store(false, std::memory_order_release);
		m_CV.notify_all();
		for (std::thread& worker : m_Workers)
		{
			if (worker.joinable())
				worker.join();
		}
		m_Workers.clear();

		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Tasks.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_MainQueueMutex);
			m_MainQueue.clear();
		}
		m_Arenas.clear();
		m_Initialized = false;
	}

	int JobSystem::CurrentLane() const
	{
		return t_Lane;
	}

	bool JobSystem::OnWorkerThread() const
	{
		return t_Lane > 0;
	}

	bool JobSystem::PopTask(Task& out)
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (m_Tasks.empty())
			return false;
		out = std::move(m_Tasks.front());
		m_Tasks.pop_front();
		return true;
	}

	void JobSystem::RunTask(Task& task)
	{
		{
			BLU_PROFILE_SCOPE(task.Zone ? task.Zone : "Job");
			if (task.Fn)
				task.Fn();
		}
		if (task.Counter)
			task.Counter->Remaining.fetch_sub(1, std::memory_order_acq_rel);
	}

	void JobSystem::WorkerLoop(int laneIndex)
	{
		t_Lane = laneIndex;
		while (true)
		{
			Task task;
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_CV.wait(lock, [this]() {
					return !m_Tasks.empty() || !m_Running.load(std::memory_order_acquire);
				});
				if (!m_Running.load(std::memory_order_acquire) && m_Tasks.empty())
					return;
				task = std::move(m_Tasks.front());
				m_Tasks.pop_front();
			}
			RunTask(task);
		}
	}

	JobHandle JobSystem::Submit(JobFn fn, const char* zone)
	{
		auto counter = std::make_shared<JobCounter>();

		if (!m_Initialized || m_Workers.empty())
		{
			// No pool: run inline so callers work in headless/uninitialized contexts.
			Task inlineTask{ std::move(fn), counter, zone };
			counter->Remaining.store(1, std::memory_order_relaxed);
			RunTask(inlineTask);
			return JobHandle(counter);
		}

		counter->Remaining.store(1, std::memory_order_release);
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Tasks.push_back(Task{ std::move(fn), counter, zone });
		}
		m_CV.notify_one();
		return JobHandle(counter);
	}

	JobHandle JobSystem::ParallelFor(uint32_t count, uint32_t grain,
		const std::function<void(uint32_t, uint32_t)>& body, const char* zone)
	{
		auto counter = std::make_shared<JobCounter>();
		if (count == 0)
			return JobHandle(counter); // already complete (Remaining == 0)

		if (grain == 0)
			grain = 1;

		// Inline when there's no pool or the work fits one chunk.
		if (!m_Initialized || m_Workers.empty() || count <= grain)
		{
			BLU_PROFILE_SCOPE(zone ? zone : "ParallelFor");
			body(0, count);
			return JobHandle(counter);
		}

		const uint32_t chunks = (count + grain - 1) / grain;
		counter->Remaining.store((int)chunks, std::memory_order_release);
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			for (uint32_t c = 0; c < chunks; ++c)
			{
				const uint32_t begin = c * grain;
				const uint32_t end = std::min(begin + grain, count);
				m_Tasks.push_back(Task{ [body, begin, end]() { body(begin, end); }, counter, zone });
			}
		}
		m_CV.notify_all();
		return JobHandle(counter);
	}

	void JobSystem::Wait(const JobHandle& handle)
	{
		if (!handle.IsValid())
			return;

		// Help-run queued tasks until this handle's counter drains. If nothing is runnable yet
		// (another thread holds the work), yield rather than busy-spin.
		while (!handle.IsComplete())
		{
			Task task;
			if (PopTask(task))
				RunTask(task);
			else
				std::this_thread::yield();
		}
	}

	void JobSystem::EnqueueMainThread(JobFn fn)
	{
		std::lock_guard<std::mutex> lock(m_MainQueueMutex);
		m_MainQueue.push_back(std::move(fn));
	}

	void JobSystem::DrainMainThreadQueue()
	{
		// Swap out the pending list so callbacks may enqueue further main-thread work for next frame.
		std::vector<JobFn> pending;
		{
			std::lock_guard<std::mutex> lock(m_MainQueueMutex);
			pending.swap(m_MainQueue);
		}
		for (JobFn& fn : pending)
		{
			if (fn)
				fn();
		}
	}

	size_t JobSystem::MainThreadQueueDepth() const
	{
		std::lock_guard<std::mutex> lock(m_MainQueueMutex);
		return m_MainQueue.size();
	}

	FrameArena& JobSystem::WorkerArena()
	{
		int lane = t_Lane;
		if (lane < 0 || lane >= (int)m_Arenas.size())
			lane = 0; // foreign/unknown thread → fall back to the main lane
		return *m_Arenas[(size_t)lane];
	}

	void JobSystem::ResetWorkerArenas()
	{
		for (std::unique_ptr<FrameArena>& arena : m_Arenas)
			arena->Reset();
	}

	size_t JobSystem::PendingTaskCount() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Tasks.size();
	}
}
