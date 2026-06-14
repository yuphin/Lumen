#include "ThreadPool.h"
#include "Framework/Base/OS.h"

namespace tp {

static constexpr u32 MAX_THREADS = 64;
static constexpr u32 MAX_QUEUED_JOBS = 4096;

struct QueuedJob {
	Job job;
	JobCounter* counter;
};

static bool _stopping = true;
static QueuedJob _work_queue[MAX_QUEUED_JOBS] = {};
static u32 _queue_read = 0;
static u32 _queue_write = 0;
static u32 _queue_count = 0;
static os::Mutex _queue_mutex;
static os::ConditionVariable _cv;
static os::Thread _threads[MAX_THREADS];
static u32 _thread_count = 0;

static bool pop_job(QueuedJob& queued_job) {
	if (_queue_count == 0) return false;
	queued_job = _work_queue[_queue_read];
	_queue_read = (_queue_read + 1) % MAX_QUEUED_JOBS;
	--_queue_count;
	return true;
}

static void execute(QueuedJob queued_job) {
	queued_job.job.procedure(queued_job.job.data);
	{
		os::ScopedLock lock(_queue_mutex);
		LUMEN_ASSERT(queued_job.counter->remaining > 0, "ThreadPool job counter underflow");
		--queued_job.counter->remaining;
	}
	_cv.notify_all();
}

static void worker(void*) {
	for (;;) {
		QueuedJob queued_job = {};
		_queue_mutex.lock();
		while (_queue_count == 0 && !_stopping) {
			_cv.wait(_queue_mutex);
		}
		if (_stopping && _queue_count == 0) {
			_queue_mutex.unlock();
			return;
		}
		pop_job(queued_job);
		_queue_mutex.unlock();
		execute(queued_job);
	}
}

void init() {
	{
		os::ScopedLock lock(_queue_mutex);
		_stopping = false;
		_queue_read = 0;
		_queue_write = 0;
		_queue_count = 0;
	}

	_thread_count = os::processor_count();
	if (_thread_count == 0) {
		_thread_count = 1;
	} else if (_thread_count > MAX_THREADS) {
		_thread_count = MAX_THREADS;
	}

	for (u32 i = 0; i < _thread_count; ++i) {
		if (!os::thread_start(_threads[i], worker, nullptr)) {
			LUMEN_ERROR("Failed to start ThreadPool worker");
		}
		char name[16] = {};
		stbsp_snprintf(name, sizeof(name), "LumenWorker %u", i);
		os::thread_set_name(_threads[i], name);
	}
}

void submit(Job job, JobCounter& counter) {
	LUMEN_ASSERT(job.procedure, "Cannot submit an empty ThreadPool job");
	QueuedJob queued_job = {job, &counter};
	bool execute_inline = false;

	_queue_mutex.lock();
	if (_stopping) {
		_queue_mutex.unlock();
		LUMEN_ERROR("ThreadPool has been terminated");
		return;
	}
	++counter.remaining;
	if (_queue_count == MAX_QUEUED_JOBS) {
		execute_inline = true;
	} else {
		_work_queue[_queue_write] = queued_job;
		_queue_write = (_queue_write + 1) % MAX_QUEUED_JOBS;
		++_queue_count;
	}
	_queue_mutex.unlock();

	if (execute_inline) {
		execute(queued_job);
	} else {
		_cv.notify_one();
	}
}

void wait(JobCounter& counter) {
	for (;;) {
		QueuedJob queued_job = {};
		_queue_mutex.lock();
		if (counter.remaining == 0) {
			_queue_mutex.unlock();
			return;
		}
		if (pop_job(queued_job)) {
			_queue_mutex.unlock();
			execute(queued_job);
			continue;
		}
		_cv.wait(_queue_mutex);
		_queue_mutex.unlock();
	}
}

void destroy() {
	{
		os::ScopedLock lock(_queue_mutex);
		_stopping = true;
	}
	_cv.notify_all();
	for (u32 i = 0; i < _thread_count; ++i) {
		os::thread_join(_threads[i]);
	}
	_thread_count = 0;
}

}  // namespace tp
