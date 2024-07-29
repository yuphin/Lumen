#include "../LumenPCH.h"
#include "VulkanContext.h"

namespace vk {
VulkanContext _context;

VulkanContext& context() { return _context; }
};	// namespace VulkanContext
