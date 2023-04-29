#pragma once
#include "../LumenPCH.h"
class VulkanSyncronization {
   public:
	   static std::mutex queue_mutex;
	   static std::atomic<uint32_t> available_command_pools;
	   static std::condition_variable cv;
};
