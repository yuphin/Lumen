#pragma once
#include "Framework/Base/OS.h"

class ThreadPool {
   public:
	using JobProcedure = void (*)(void*);

	struct Job {
		JobProcedure procedure = nullptr;
		void* data = nullptr;
	};

	struct JobCounter {
		u32 remaining = 0;
	};

	static void init();
	static void destroy();
	static void submit(Job job, JobCounter& counter);
	static void wait(JobCounter& counter);

   private:
	struct QueuedJob {
		Job job;
		JobCounter* counter;
	};

	static constexpr u32 MAX_THREADS = 64;
	static constexpr u32 MAX_QUEUED_JOBS = 4096;

	static void worker(void*);
	static void execute(QueuedJob queued_job);
	static bool pop_job(QueuedJob& queued_job);

	static bool stopping;
	static QueuedJob work_queue[MAX_QUEUED_JOBS];
	static u32 queue_read;
	static u32 queue_write;
	static u32 queue_count;
	static os::Mutex queue_mutex;
	static os::ConditionVariable cv;
	static os::Thread threads[MAX_THREADS];
	static u32 thread_count;
};
