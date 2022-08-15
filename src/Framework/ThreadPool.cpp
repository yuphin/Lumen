#include "LumenPCH.h"
#include "ThreadPool.h"
std::atomic_bool ThreadPool::done;
std::queue<std::function<void()>> ThreadPool::work_queue;
std::mutex ThreadPool::queue_mutex;
std::condition_variable ThreadPool::cv;
std::vector<std::thread> ThreadPool::threads;
void ThreadPool::init() {
	uint32_t thread_count = std::thread::hardware_concurrency();
	done = false;
	try {
		threads.reserve(thread_count);
		for (uint32_t i = 0; i < thread_count; i++) {
			threads.emplace_back([] {
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
			});
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
}
