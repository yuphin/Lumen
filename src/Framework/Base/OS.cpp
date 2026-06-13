#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#endif // defined(_WIN32) || defined(_WIN64)
#include "OS.h"
#include "Framework/Base/String.h"
namespace os {

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI thread_entry(void* raw_thread) {
	Thread* thread = (Thread*)raw_thread;
	thread->procedure(thread->data);
	return 0;
}
#else
static void* thread_entry(void* raw_thread) {
	Thread* thread = (Thread*)raw_thread;
	thread->procedure(thread->data);
	return nullptr;
}
#endif

u32 processor_count() {
#if defined(_WIN32) || defined(_WIN64)
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwNumberOfProcessors;
#else
	long count = sysconf(_SC_NPROCESSORS_ONLN);
	return count > 0 ? (u32)count : 1;
#endif
}

bool thread_start(Thread& thread, ThreadProcedure procedure, void* data) {
	thread.procedure = procedure;
	thread.data = data;
#if defined(_WIN32) || defined(_WIN64)
	thread.handle = CreateThread(nullptr, 0, thread_entry, &thread, 0, nullptr);
	if (!thread.handle) {
		thread.procedure = nullptr;
		thread.data = nullptr;
		return false;
	}
#else
	if (pthread_create(&thread.native, nullptr, thread_entry, &thread) != 0) {
		thread.procedure = nullptr;
		thread.data = nullptr;
		return false;
	}
	thread.started = true;
#endif
	return true;
}

void thread_join(Thread& thread) {
#if defined(_WIN32) || defined(_WIN64)
	if (!thread.handle) return;
	WaitForSingleObject((HANDLE)thread.handle, INFINITE);
	CloseHandle((HANDLE)thread.handle);
	thread.handle = nullptr;
#else
	if (!thread.started) return;
	pthread_join(thread.native, nullptr);
	thread.started = false;
#endif
	thread.procedure = nullptr;
	thread.data = nullptr;
}

void thread_set_name(Thread& thread, const char* name) {
#if defined(_WIN32) || defined(_WIN64)
	if (!thread.handle) return;
	wchar_t wide_name[64] = {};
	u32 i = 0;
	for (; name[i] && i + 1 < ARRAY_SIZE(wide_name); ++i) {
		wide_name[i] = (wchar_t)(u8)name[i];
	}
	wide_name[i] = L'\0';
	SetThreadDescription((HANDLE)thread.handle, wide_name);
#elif defined(__linux__)
	if (thread.started) {
		pthread_setname_np(thread.native, name);
	}
#endif
}

#if defined(_WIN32) || defined(_WIN64)
void Mutex::lock() { AcquireSRWLockExclusive((PSRWLOCK)&native); }
void Mutex::unlock() { ReleaseSRWLockExclusive((PSRWLOCK)&native); }

void ConditionVariable::wait(Mutex& mutex) {
	SleepConditionVariableSRW((PCONDITION_VARIABLE)&native, (PSRWLOCK)&mutex.native, INFINITE, 0);
}
void ConditionVariable::notify_one() { WakeConditionVariable((PCONDITION_VARIABLE)&native); }
void ConditionVariable::notify_all() { WakeAllConditionVariable((PCONDITION_VARIABLE)&native); }

Semaphore::Semaphore(u32 initial_count, u32 max_count) {
	handle = CreateSemaphoreA(nullptr, (LONG)initial_count, (LONG)max_count, nullptr);
	LUMEN_ASSERT(handle, "Failed to create semaphore");
}
Semaphore::~Semaphore() {
	if (handle) CloseHandle((HANDLE)handle);
}
void Semaphore::acquire() { WaitForSingleObject((HANDLE)handle, INFINITE); }
void Semaphore::release() { ReleaseSemaphore((HANDLE)handle, 1, nullptr); }
#else
Mutex::Mutex() { pthread_mutex_init(&native, nullptr); }
Mutex::~Mutex() { pthread_mutex_destroy(&native); }
void Mutex::lock() { pthread_mutex_lock(&native); }
void Mutex::unlock() { pthread_mutex_unlock(&native); }

ConditionVariable::ConditionVariable() { pthread_cond_init(&native, nullptr); }
ConditionVariable::~ConditionVariable() { pthread_cond_destroy(&native); }
void ConditionVariable::wait(Mutex& mutex) { pthread_cond_wait(&native, &mutex.native); }
void ConditionVariable::notify_one() { pthread_cond_signal(&native); }
void ConditionVariable::notify_all() { pthread_cond_broadcast(&native); }

Semaphore::Semaphore(u32 initial_count, u32 max_count) {
	(void)max_count;
	i32 result = sem_init(&native, 0, initial_count);
	LUMEN_ASSERT(result == 0, "Failed to create semaphore");
}
Semaphore::~Semaphore() { sem_destroy(&native); }
void Semaphore::acquire() {
	while (sem_wait(&native) != 0 && errno == EINTR) {
	}
}
void Semaphore::release() { sem_post(&native); }
#endif

u64 get_page_size() {
#if defined(_WIN32) || defined(_WIN64)
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwPageSize;
#else
	return sysconf(_SC_PAGE_SIZE);
#endif	// defined(_WIN32) || defined(_WIN64)
}

void reserve(void* ptr, u64 size) {
#if defined(_WIN32) || defined(_WIN64)
	void* res = VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_NOACCESS);
	assert(res == ptr);
#else
	void* result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(result == ptr);
#endif
}

void* reserve(u64 reserve_size) {
	void* data_base;
#if defined(_WIN32) || defined(_WIN64)
	data_base = VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
#else
	data_base = mmap(0, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (data_base == MAP_FAILED) {
		data_base = nullptr;
	}
#endif	// defined(_WIN32) || defined(_WIN64)
	return data_base;
}
bool commit(void* ptr, u64 commit_size) {
#if defined(_WIN32) || defined(_WIN64)
	return (VirtualAlloc(ptr, commit_size, MEM_COMMIT, PAGE_READWRITE) != 0);
#else
	return mprotect(ptr, commit_size, PROT_READ | PROT_WRITE) == 0;
#endif	// defined(_WIN32) || defined(_WIN64)
}

u64 file_read(FileHandle handle, void* out_data, u64 size) {
	if (handle == 0) {
		return 0;
	}
#if defined(_WIN32) || defined(_WIN64)
	HANDLE os_handle = (HANDLE)handle;
	if (size == U64_MAX) {
		GetFileSizeEx(os_handle, (PLARGE_INTEGER)&size);
	}
	u64 total_bytes_read = 0;

	for (u64 offset = 0; offset < size; offset += total_bytes_read) {
		DWORD bytes_read;
		OVERLAPPED overlapped = {0};
		overlapped.Offset = (offset & 0x00000000ffffffffull);
		overlapped.OffsetHigh = (offset & 0xffffffff00000000ull) >> 32;
		u64 remaining = size - offset;
		if (remaining > U32_MAX) {
			remaining = U32_MAX;
		}
		if (!ReadFile(os_handle, (u8*)out_data + offset, (DWORD)remaining, &bytes_read, &overlapped)) {
			return 0;
		}
		total_bytes_read += bytes_read;
	}
	return total_bytes_read;

#else
	int fd = (int)handle;
	if (size == U64_MAX) {
		struct stat file_stat;
		if (fstat(fd, &file_stat) == 0) {
			size = file_stat.st_size;
		} else {
			return 0;
		}
	}
	u64 total_bytes_read = 0;
	u64 bytes_left = size;
	while (bytes_left > 0) {
		ssize_t result = pread(fd, (u8*)out_data + total_bytes_read, bytes_left, total_bytes_read);
		if (result > 0) {
			total_bytes_read += (u64)result;
			bytes_left -= (u64)result;
		} else if (result == 0) {
			break;
		} else if (errno != EINTR) {
			return 0;
		}
	}
	return total_bytes_read;
#endif	// defined(_WIN32) || defined(_WIN64)
}

u64 file_write(FileHandle handle, void* in_data, u64 size) {
	if (handle == 0) {
		return 0;
	}
#if defined(_WIN32) || defined(_WIN64)
	HANDLE win_handle = (HANDLE)handle;
	u64 src_offset = 0;
	u64 dst_offset = 0;
	for (;;) {
		DWORD bytes_left = DWORD(size - src_offset);
		if (bytes_left == 0) {
			break;
		}
		DWORD bytes_written = 0;
		void* data = (u8*)in_data + src_offset;
		OVERLAPPED overlapped = {0};
		overlapped.Offset = (dst_offset & 0x00000000ffffffffull);
		overlapped.OffsetHigh = (dst_offset & 0xffffffff00000000ull) >> 32;
		BOOL success = WriteFile(win_handle, data, bytes_left, &bytes_written, &overlapped);
		if (success == 0) {
			break;
		}
		src_offset += bytes_written;
		dst_offset += bytes_written;
	}
	return src_offset;
#else
	int fd = (int)handle;
	u64 src_offset = 0;
	u64 dst_offset = 0;
	for (;;) {
		u64 bytes_left = size - src_offset;
		if (bytes_left == 0) break;
		ssize_t n = pwrite(fd, (u8*)in_data + src_offset, bytes_left, (off_t)dst_offset);
		if (n < 0) {
			if (errno == EINTR) continue;
			break;
		}
		src_offset += (u64)n;
		dst_offset += (u64)n;
	}
	return src_offset;
#endif
}

FileHandle file_open(const lm::String& path, AccessFlags access_flags) {
	assert(path.is_cstr());
#if defined(_WIN32) || defined(_WIN64)
	DWORD access = 0;
	DWORD creation_disposition = OPEN_EXISTING;
	if (access_flags & AccessFlag_Read) {
		access |= GENERIC_READ;
	}
	if (access_flags & AccessFlag_Write) {
		access |= GENERIC_WRITE;
		creation_disposition = CREATE_ALWAYS;
	};
	if (access_flags & AccessFlag_Execute) {
		access |= GENERIC_EXECUTE;
	}

	HANDLE file_handle = CreateFileA(path.data, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, creation_disposition,
									 FILE_ATTRIBUTE_NORMAL, NULL);

	if (file_handle == INVALID_HANDLE_VALUE) {
		return 0;
	}
	return (FileHandle)file_handle;
#else
	int flags = 0;
	if ((access_flags & AccessFlag_Read) && (access_flags & AccessFlag_Write)) {
		flags |= O_RDWR | O_CREAT | O_TRUNC;
	} else if (access_flags & AccessFlag_Read) {
		flags |= O_RDONLY;
	} else if (access_flags & AccessFlag_Write) {
		flags |= O_WRONLY | O_CREAT | O_TRUNC;
	}
	if (access_flags & AccessFlag_Append) {
		flags |= O_APPEND;
	}

	int fd = open(path.data, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		return 0;
	}
	return (FileHandle)fd;

#endif	// defined(_WIN32) || defined(_WIN64)
}

void file_close(FileHandle handle) {
	if (handle == 0) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	CloseHandle((HANDLE)handle);
#else
	close((int)handle);
#endif
}

FileProperties file_properties(FileHandle handle) {
	if (handle == 0) {
		return {};
	}

#if defined(_WIN32) || defined(_WIN64)
	HANDLE file_handle = (HANDLE)handle;
	BY_HANDLE_FILE_INFORMATION info;
	BOOL success = GetFileInformationByHandle(file_handle, &info);
	FileProperties properties = {};
	if (success) {
		u32 size_low = info.nFileSizeLow;
		u32 size_high = info.nFileSizeHigh;
		u64 size = ((u64)size_high << 32) | size_low;
		properties = {
			.size = size,
			.created = ((u64)info.ftCreationTime.dwHighDateTime << 32) | info.ftCreationTime.dwLowDateTime,
			.modified = ((u64)info.ftLastWriteTime.dwHighDateTime << 32) | info.ftLastWriteTime.dwLowDateTime};
	}
	return properties;
#else
	int fd = (int)handle;
	struct stat file_stat;
	if (fstat(fd, &file_stat) == 0) {
		FileProperties properties = {
			.size = file_stat.st_size, .created = file_stat.st_ctime, .modified = file_stat.st_mtime};
		return properties;
	}
	return {};
#endif
}

}  // namespace os
