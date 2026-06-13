#include "ThreadPool.h"
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif //  defined(_WIN32) || defined(_WIN64)

std::atomic_bool ThreadPool::done;
std::queue<std::function<void()>> ThreadPool::work_queue;
std::mutex ThreadPool::queue_mutex;
std::condition_variable ThreadPool::cv;
lm::SmallArray<std::thread, 64> ThreadPool::threads;
void ThreadPool::init() {
	u32 thread_count = std::thread::hardware_concurrency();
	if (thread_count == 0) {
		thread_count = 1;
	} else if (thread_count > threads.capacity()) {
		thread_count = static_cast<u32>(threads.capacity());
	}
	done = false;
	try {
		for (u32 i = 0; i < thread_count; i++) {
			threads.push_back_move(std::thread([i] {
#ifdef _WIN32
				wchar_t threadName[64];
				swprintf(threadName, 64, L"LumenWorker %d", i);
				SetThreadDescription(GetCurrentThread(), threadName);
#endif
				while (true) {
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(queue_mutex);
						cv.wait(lock, [] { return !work_queue.empty() || done; });
						if (done && work_queue.empty()) {
							break;
						}
						task = std::move(work_queue.front());
						work_queue.pop();
					}
					task();
				}
			}));
		}
	} catch (const std::exception& ex) {
		LUMEN_ERROR(ex.what());
	}
}

void ThreadPool::destroy() {
	done = true;
	cv.notify_all();
	for (auto& thread : threads) {
		thread.join();
	}
	threads.clear();
}
