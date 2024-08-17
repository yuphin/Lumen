#include "../LumenPCH.h"
#include "VulkanSyncronization.h"
std::mutex VulkanSyncronization::queue_mutex;
std::mutex VulkanSyncronization::command_pool_mutex;
uint64_t VulkanSyncronization::available_command_pools = UINT64_MAX;
std::counting_semaphore<64> VulkanSyncronization::command_pool_semaphore{64};
