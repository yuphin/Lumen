#pragma once
#include "lmhpch.h"
constexpr void check(VkResult result, const char* msg) {
	if (result != VK_SUCCESS) {
		LUMEN_ERROR(msg);
	}
}



