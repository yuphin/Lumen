#define VMA_IMPLEMENTATION
#include "../LumenPCH.h"
#include <vk_mem_alloc.h>
#include "VulkanContext.h"

namespace vk {
VulkanContext _context;

VulkanContext& context() { return _context; }
};	// namespace VulkanContext
