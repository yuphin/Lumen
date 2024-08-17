#pragma once
#include "../LumenPCH.h"
struct VulkanSyncronization {
	   static std::mutex queue_mutex;
	   static std::mutex command_pool_mutex;
	   static uint64_t available_command_pools;
	   static std::condition_variable cv;
	   static std::counting_semaphore<64> command_pool_semaphore;
};
