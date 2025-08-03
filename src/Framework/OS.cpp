#include "../LumenPCH.h"
#include "OS.h"

namespace os {
size_t get_page_size() {
#if defined(_WIN32) || defined(_WIN64)
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwPageSize;
#else
	return sysconf(_SC_PAGE_SIZE);
#endif	// defined(_WIN32) || defined(_WIN64)
}
void* reserve(size_t reserve_size) {
	void* data_base;
#if defined(_WIN32) || defined(_WIN64)
	data_base = VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
#else
	data_base = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED) {
		result = 0;
	}
#endif	// defined(_WIN32) || defined(_WIN64)
	return data_base;
}
bool commit(void* ptr, size_t commit_size) {
#if defined(_WIN32) || defined(_WIN64)
	return (VirtualAlloc(ptr, commit_size, MEM_COMMIT, PAGE_READWRITE) != 0);
#else
	mprotect(ptr, size, PROT_READ | PROT_WRITE);
	return true;
#endif	// defined(_WIN32) || defined(_WIN64)
}
}  // namespace os