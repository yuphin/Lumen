#pragma once
#if !defined(_WIN32) && !defined(_WIN64)
#include <pthread.h>
#include <semaphore.h>
#endif

namespace lm {
struct String;
}
namespace os {
typedef u32 AccessFlags;
typedef u64 FileHandle;
using ThreadProcedure = void (*)(void*);

#if defined(_WIN32) || defined(_WIN64)
struct Thread {
	void* handle = nullptr;
	ThreadProcedure procedure = nullptr;
	void* data = nullptr;
};

struct Mutex {
	void* native = nullptr;

	Mutex() = default;
	Mutex(const Mutex&) = delete;
	Mutex& operator=(const Mutex&) = delete;

	void lock();
	void unlock();
};

struct ConditionVariable {
	void* native = nullptr;

	ConditionVariable() = default;
	ConditionVariable(const ConditionVariable&) = delete;
	ConditionVariable& operator=(const ConditionVariable&) = delete;

	void wait(Mutex& mutex);
	void notify_one();
	void notify_all();
};

struct Semaphore {
	void* handle = nullptr;

	Semaphore(u32 initial_count, u32 max_count);
	~Semaphore();
	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;

	void acquire();
	void release();
};
#else
struct Thread {
	pthread_t native = {};
	ThreadProcedure procedure = nullptr;
	void* data = nullptr;
	bool started = false;
};

struct Mutex {
	pthread_mutex_t native = {};

	Mutex();
	~Mutex();
	Mutex(const Mutex&) = delete;
	Mutex& operator=(const Mutex&) = delete;

	void lock();
	void unlock();
};

struct ConditionVariable {
	pthread_cond_t native = {};

	ConditionVariable();
	~ConditionVariable();
	ConditionVariable(const ConditionVariable&) = delete;
	ConditionVariable& operator=(const ConditionVariable&) = delete;

	void wait(Mutex& mutex);
	void notify_one();
	void notify_all();
};

struct Semaphore {
	sem_t native = {};

	Semaphore(u32 initial_count, u32 max_count);
	~Semaphore();
	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;

	void acquire();
	void release();
};
#endif

struct ScopedLock {
	Mutex& mutex;
	bool locked;

	explicit ScopedLock(Mutex& mutex, bool lock = true) : mutex(mutex), locked(lock) {
		if (locked) mutex.lock();
	}
	~ScopedLock() {
		if (locked) mutex.unlock();
	}
	ScopedLock(const ScopedLock&) = delete;
	ScopedLock& operator=(const ScopedLock&) = delete;
};

enum {
	AccessFlag_Read = (1 << 0),
	AccessFlag_Write = (1 << 1),
	AccessFlag_Execute = (1 << 2),
	AccessFlag_Append = (1 << 3),
	AccessFlag_ShareRead = (1 << 4),
	AccessFlag_ShareWrite = (1 << 5),
	AccessFlag_Inherited = (1 << 6),
};

struct FileProperties {
	u64 size;
	u64 created;
	u64 modified;
};
u32 processor_count();
bool thread_start(Thread& thread, ThreadProcedure procedure, void* data);
void thread_join(Thread& thread);
void thread_set_name(Thread& thread, const char* name);
u64 get_page_size();
void reserve(void* ptr, u64 size);
void* reserve(u64 reserve_size);
bool commit(void* ptr, u64 commit_size);
u64 file_read(FileHandle handle, void* out_data, u64 size = U64_MAX);
u64 file_write(FileHandle handle, void* in_data, u64 size);
FileHandle file_open(const lm::String& path, AccessFlags access_flags);
void file_close(FileHandle handle);
FileProperties file_properties(FileHandle handle);

}  // namespace os
