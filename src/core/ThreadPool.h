#pragma once
#include "lmhpch.h"


class ThreadPool {
public:
	template<typename FunctionType, typename... Args>
	static auto submit(FunctionType&& f, Args&&...args);
	static void init();
	static void destroy();
private:
	static std::atomic_bool done;
	static std::queue<std::function<void()>> work_queue;
	static std::mutex queue_mutex;
	static std::condition_variable cv;
	static std::vector<std::thread> threads;

};

template<typename FunctionType, typename ...Args>
 auto ThreadPool::submit(FunctionType&& f, Args && ...args) {
	using result_type = std::invoke_result_t<FunctionType, Args...>;
	auto task = std::make_unique<std::packaged_task<result_type()>>(
		std::bind(std::forward<FunctionType>(f), std::forward<Args>(args)...)
		);
	auto result = task->get_future();
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (done) {
			LUMEN_ERROR("ThreadPool has been terminated");
		}
		auto task_ptr = task.get();
		work_queue.emplace([task_ptr]() {
			(*task_ptr)();
			});
	}
	cv.notify_one();
	return result;



}
