#include "LumenPCH.h"
#include "VulkanSyncronization.h"
std::mutex VulkanSyncronization::queue_mutex;
std::atomic<uint32_t> VulkanSyncronization::available_command_pools = std::thread::hardware_concurrency();
std::condition_variable VulkanSyncronization::cv;
