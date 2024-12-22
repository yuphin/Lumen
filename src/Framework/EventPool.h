#pragma once
#include "../LumenPCH.h"

namespace vk {

namespace event_pool {
VkEvent get_event(VkCommandBuffer cmd);
void reset_events();
void cleanup();

}  // namespace event_pool

}  // namespace vk
