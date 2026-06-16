#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Blu Job System — a dependency-light thread pool + task scheduler that the rest
// of the engine builds on (parallel update, async asset streaming). Mirrors the
// shape of Jolt's JobSystemThreadPool without linking Jolt.
//
// Constraints baked in:
//  • DX11 immediate context is single-threaded — GPU-affinity work runs on the
//    main thread via EnqueueMainThread() / DrainMainThreadQueue(); workers never
//    touch the device context (OnWorkerThread() asserts guard this in debug).
//  • entt is not concurrent-safe — parallel bodies only READ components and write
//    into per-entity / pre-sized external storage; never mutate registry structure.
//  • Sized hardware_concurrency()-1 so it coexists with Jolt's pool (sequential
//    phases). The waiting thread HELPS-RUN queued tasks (no idle stall, no extra
//    dedicated thread for the main thread's share of work).
// ─────────────────────────────────────────────────────────────────────────────
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Blu
{
	class FrameArena;

	using JobFn = std::function<void()>;

	// Completion counter shared by a submitted job or a parallel-for batch.
	struct JobCounter
	{
		std::atomic<int> Remaining{ 0 };
	};

	// Cheap, copyable handle used to wait on submitted work.
	class JobHandle
	{
	public:
		JobHandle() = default;
		bool IsValid() const { return static_cast<bool>(m_Counter); }
		bool IsComplete() const
		{
			return !m_Counter || m_Counter->Remaining.load(std::memory_order_acquire) == 0;
		}

	private:
		friend class JobSystem;
		explicit JobHandle(std::shared_ptr<JobCounter> counter) : m_Counter(std::move(counter)) {}
		std::shared_ptr<JobCounter> m_Counter;
	};

	class JobSystem
	{
	public:
		static JobSystem& Get();

		// workerCount < 0 => hardware_concurrency()-1 (clamped to >= 0). Call once, on the main
		// thread, after the render device exists. Safe to call when already initialized (no-op).
		void Initialize(int workerCount = -1);
		// Joins all workers. Call before the render device is destroyed.
		void Shutdown();
		bool IsInitialized() const { return m_Initialized; }

		int  WorkerCount() const { return static_cast<int>(m_Workers.size()); }
		bool OnWorkerThread() const; // true only when called from a pool worker thread

		// Run fn on the pool; returns immediately. If the pool is uninitialized/empty, runs inline.
		JobHandle Submit(JobFn fn, const char* zone = "Job");

		// Split [0,count) into ceil(count/grain) chunks run in parallel; body(begin,end) per chunk.
		// Runs inline if the pool is uninitialized/empty or count <= grain.
		JobHandle ParallelFor(uint32_t count, uint32_t grain,
		                      const std::function<void(uint32_t /*begin*/, uint32_t /*end*/)>& body,
		                      const char* zone = "ParallelFor");

		// Block until handle completes; the calling thread HELPS-RUN other queued tasks meanwhile.
		void Wait(const JobHandle& handle);

		// Main-thread dispatch queue: workers push GPU-affinity continuations here; the main thread
		// drains them once per frame (DX11-safe upload point).
		void EnqueueMainThread(JobFn fn);
		void DrainMainThreadQueue();
		size_t MainThreadQueueDepth() const;

		// Per-lane scratch arena (lane 0 = main thread, 1..N = workers). Reset once per frame.
		FrameArena& WorkerArena();
		void ResetWorkerArenas();

		size_t PendingTaskCount() const;

	private:
		JobSystem() = default;
		~JobSystem();
		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		struct Task
		{
			JobFn Fn;
			std::shared_ptr<JobCounter> Counter;
			const char* Zone = "Job";
		};

		void WorkerLoop(int laneIndex);
		bool PopTask(Task& out);      // false when no task is queued
		void RunTask(Task& task);     // runs Fn (profiled) then decrements its counter
		int  CurrentLane() const;     // 0 = main, 1..N = worker; -1 = foreign thread

		std::vector<std::thread>      m_Workers;
		std::deque<Task>              m_Tasks;
		mutable std::mutex            m_Mutex;
		std::condition_variable       m_CV;
		std::atomic<bool>             m_Running{ false };
		bool                          m_Initialized = false;

		std::vector<JobFn>            m_MainQueue;
		mutable std::mutex            m_MainQueueMutex;

		std::vector<std::unique_ptr<FrameArena>> m_Arenas; // size = WorkerCount()+1 (index 0 = main)
	};
}
