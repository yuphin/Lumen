#include "ThreadPool.h"
#include <stb/stb_sprintf.h>

bool ThreadPool::stopping = true;
ThreadPool::QueuedJob ThreadPool::work_queue[MAX_QUEUED_JOBS] = {};
u32 ThreadPool::queue_read = 0;
u32 ThreadPool::queue_write = 0;
u32 ThreadPool::queue_count = 0;
os::Mutex ThreadPool::queue_mutex;
os::ConditionVariable ThreadPool::cv;
os::Thread ThreadPool::threads[MAX_THREADS];
u32 ThreadPool::thread_count = 0;

bool ThreadPool::pop_job(QueuedJob& queued_job) {
	if (queue_count == 0) return false;
	queued_job = work_queue[queue_read];
	queue_read = (queue_read + 1) % MAX_QUEUED_JOBS;
	--queue_count;
	return true;
}

void ThreadPool::execute(QueuedJob queued_job) {
	queued_job.job.procedure(queued_job.job.data);
	{
		os::ScopedLock lock(queue_mutex);
		LUMEN_ASSERT(queued_job.counter->remaining > 0, "ThreadPool job counter underflow");
		--queued_job.counter->remaining;
	}
	cv.notify_all();
}

void ThreadPool::worker(void*) {
	for (;;) {
		QueuedJob queued_job = {};
		queue_mutex.lock();
		while (queue_count == 0 && !stopping) {
			cv.wait(queue_mutex);
		}
		if (stopping && queue_count == 0) {
			queue_mutex.unlock();
			return;
		}
		pop_job(queued_job);
		queue_mutex.unlock();
		execute(queued_job);
	}
}

void ThreadPool::init() {
	{
		os::ScopedLock lock(queue_mutex);
		stopping = false;
		queue_read = 0;
		queue_write = 0;
		queue_count = 0;
	}

	thread_count = os::processor_count();
	if (thread_count == 0) {
		thread_count = 1;
	} else if (thread_count > MAX_THREADS) {
		thread_count = MAX_THREADS;
	}

	for (u32 i = 0; i < thread_count; ++i) {
		if (!os::thread_start(threads[i], worker, nullptr)) {
			LUMEN_ERROR("Failed to start ThreadPool worker");
		}
		char name[16] = {};
		stbsp_snprintf(name, sizeof(name), "LumenWorker %u", i);
		os::thread_set_name(threads[i], name);
	}
}

void ThreadPool::submit(Job job, JobCounter& counter) {
	LUMEN_ASSERT(job.procedure, "Cannot submit an empty ThreadPool job");
	QueuedJob queued_job = {job, &counter};
	bool execute_inline = false;

	queue_mutex.lock();
	if (stopping) {
		queue_mutex.unlock();
		LUMEN_ERROR("ThreadPool has been terminated");
		return;
	}
	++counter.remaining;
	if (queue_count == MAX_QUEUED_JOBS) {
		execute_inline = true;
	} else {
		work_queue[queue_write] = queued_job;
		queue_write = (queue_write + 1) % MAX_QUEUED_JOBS;
		++queue_count;
	}
	queue_mutex.unlock();

	if (execute_inline) {
		execute(queued_job);
	} else {
		cv.notify_one();
	}
}

void ThreadPool::wait(JobCounter& counter) {
	for (;;) {
		QueuedJob queued_job = {};
		queue_mutex.lock();
		if (counter.remaining == 0) {
			queue_mutex.unlock();
			return;
		}
		if (pop_job(queued_job)) {
			queue_mutex.unlock();
			execute(queued_job);
			continue;
		}
		cv.wait(queue_mutex);
		queue_mutex.unlock();
	}
}

void ThreadPool::destroy() {
	{
		os::ScopedLock lock(queue_mutex);
		stopping = true;
	}
	cv.notify_all();
	for (u32 i = 0; i < thread_count; ++i) {
		os::thread_join(threads[i]);
	}
	thread_count = 0;
}
