#pragma once
#include <vulkan/vulkan.h>
#include "VKStructs.h"
#include <stdexcept>

constexpr void check(VkResult result, const char* msg) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(msg);
	}
}



