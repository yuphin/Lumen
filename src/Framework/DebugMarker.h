#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
namespace DebugMarker {
	PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
	PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;
	inline void setup(VkDevice device) {
		pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
		pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
		pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
		pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
		pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));
	}
	inline void set_resource_name(VkDevice device, uint64_t obj,
								  const char* name, VkObjectType type) {
		/*	if()
			VkDebugUtilsObjectNameInfoEXT debug_utils_name{
				VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				nullptr, type, obj, name
			};
			vkSetDebugUtilsObjectNameEXT(device, &debug_utils_name);*/
	}
	inline void begin_region(VkCommandBuffer cmd, const char* name, glm::vec4 color) {
		/*	VkDebugMarkerMarkerInfoEXT info = {};
			info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			memcpy(info.color, &color[0], sizeof(float) * 4);
			info.pMarkerName = name;
			vkCmdDebugMarkerBeginEXT(cmd, &info);*/
	}

	inline void end_region(VkCommandBuffer cmd) {
		//vkCmdDebugMarkerEndEXT(cmd);
	}
	inline void insert(VkCommandBuffer cmd, const char* name, glm::vec4 color) {
		/*	VkDebugMarkerMarkerInfoEXT info = {};
			info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
			memcpy(info.color, &color[0], sizeof(float) * 4);
			info.pMarkerName = name;
			vkCmdDebugMarkerInsertEXT(cmd, &info);*/
	}
}

