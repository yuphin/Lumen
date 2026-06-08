#include "Framework/VulkanContext.h"
#include "Framework/GPUQueryManager.h"
#include "RenderGraph.h"
#include "VulkanContext.h"
#include <volk/volk.h>
#include "VulkanBase.h"
#include "CommandBuffer.h"
#include "PersistentResourceManager.h"
#include "Window.h"
#include <unordered_map>
#include <unordered_set>

namespace vk {

static constexpr u64 MAX_SURFACE_FORMATS = 64;
static constexpr u64 MAX_PRESENT_MODES = 16;
static constexpr u64 MAX_VALIDATION_LAYERS = 8;
static constexpr u64 MAX_DEVICE_EXTENSIONS = 64;
static constexpr u64 MAX_INSTANCE_EXTENSIONS = 64;
static constexpr u64 MAX_PHYSICAL_DEVICES = 8;
static constexpr u64 MAX_QUEUE_FAMILIES = 16;
static constexpr u64 MAX_DEVICE_QUEUES = 8;
static constexpr u64 MAX_LAYER_PROPERTIES = 64;
static constexpr u64 MAX_EXTENSION_PROPERTIES = 512;

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities = {};
	lm::SmallArray<VkSurfaceFormatKHR, MAX_SURFACE_FORMATS> formats;
	lm::SmallArray<VkPresentModeKHR, MAX_PRESENT_MODES> present_modes;
};

lm::SmallArray<const char*, MAX_VALIDATION_LAYERS> _validation_layers_lst;
lm::SmallArray<const char*, MAX_DEVICE_EXTENSIONS> _device_extensions;

lm::SmallArray<VkSemaphore, MAX_FRAMES_IN_FLIGHT> _image_available_sem;
lm::SmallArray<VkSemaphore, MAX_SWAPCHAIN_IMAGES> _render_finished_sem;
lm::SmallArray<VkFence, MAX_FRAMES_IN_FLIGHT> _in_flight_fences;
lm::SmallArray<VkFence, MAX_SWAPCHAIN_IMAGES> _images_in_flight;
lm::SmallArray<VkQueueFamilyProperties, MAX_QUEUE_FAMILIES> _queue_families;

lm::RenderGraph _rg;
VkFormat _swapchain_format;

lm::SmallArray<Texture*, MAX_SWAPCHAIN_IMAGES> _swapchain_images;

bool _enable_validation_layers;

VkDescriptorPool _imgui_pool = 0;

// -------------------------------------------------------------------------------------------------
// Implementation
// -------------------------------------------------------------------------------------------------

static lm::SmallArray<const char*, MAX_INSTANCE_EXTENSIONS> get_req_extensions() {
	u32 glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	lm::SmallArray<const char*, MAX_INSTANCE_EXTENSIONS> extensions;
	for (u32 i = 0; i < glfwExtensionCount; ++i) {
		extensions.push_back(glfwExtensions[i]);
	}

	if (_enable_validation_layers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}

static QueueFamilyIndices find_queue_families(VkPhysicalDevice device) {
	QueueFamilyIndices indices;
	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_family_count, nullptr);

	lm::SmallArray<VkQueueFamilyProperties2, MAX_QUEUE_FAMILIES> queue_families2;
	queue_families2.resize(queue_family_count);
	for (auto& queue_family : queue_families2) {
		queue_family = {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2};
	}
	vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_family_count, queue_families2.data);

	_queue_families.resize(queue_family_count);
	for (u32 i = 0; i < queue_family_count; ++i) {
		_queue_families[i] = queue_families2[i].queueFamilyProperties;
	}

	i32 i = 0;
	for (const auto& queueFamily : _queue_families) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.gfx_family = i;
		}

		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
			indices.compute_family = i;
		}

		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, context().surface, &present_support);

		if (present_support) {
			indices.present_family = i;
		}

		if (indices.is_complete()) {
			break;
		}

		i++;
	}
	return indices;
}

static SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device) {
	// Basically returns present modes and surface modes in a struct

	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, context().surface, &details.capabilities);

	u32 format_cnt;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, context().surface, &format_cnt, nullptr);

	if (format_cnt != 0) {
		details.formats.resize(format_cnt);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, context().surface, &format_cnt, details.formats.data);
	}

	u32 present_mode_cnt;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, context().surface, &present_mode_cnt, nullptr);

	if (present_mode_cnt != 0) {
		details.present_modes.resize(present_mode_cnt);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, context().surface, &present_mode_cnt,
												  details.present_modes.data);
	}
	return details;
}

static VkResult vkExt_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
											 const VkAllocationCallbacks* pAllocator,
											 VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void vkExt_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
										  const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debug_messenger, pAllocator);
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
													 VkDebugUtilsMessageTypeFlagsEXT messageType,
													 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
													 void* pUserData) {
	if ((messageSeverity &
		 (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) == 0) {
		return VK_TRUE;
	}

	if ((messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) == 0) {
		LUMEN_WARN("Validation Warning: %s ", pCallbackData->pMessage);
		return VK_TRUE;
	}
	LUMEN_ERROR("Validation Error: %s ", pCallbackData->pMessage);
	return VK_FALSE;
}

static void setup_debug_messenger() {
	auto ci = debug_messenger(debug_callback);

	check(vkExt_create_debug_messenger(context().instance, &ci, nullptr, &context().debug_messenger),
		  "Failed to set up debug messenger!");
}
static void cleanup_swapchain_images() {
	for (Texture* swapchain_img : _swapchain_images) {
		prm::remove(swapchain_img);
	}
	_swapchain_images.clear();
}

static VKAPI_ATTR void VKAPI_CALL get_physical_device_properties(VkPhysicalDevice device,
																 VkPhysicalDeviceProperties* properties) {
	VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	vkGetPhysicalDeviceProperties2(device, &properties2);
	*properties = properties2.properties;
}

static VKAPI_ATTR void VKAPI_CALL get_physical_device_memory_properties(
	VkPhysicalDevice device, VkPhysicalDeviceMemoryProperties* properties) {
	VkPhysicalDeviceMemoryProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
	vkGetPhysicalDeviceMemoryProperties2(device, &properties2);
	*properties = properties2.memoryProperties;
}

static void create_allocator() {
	VmaVulkanFunctions vulkan_functions = {};
	vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkan_functions.vkGetPhysicalDeviceProperties = get_physical_device_properties;
	vulkan_functions.vkGetPhysicalDeviceMemoryProperties = get_physical_device_memory_properties;
	vulkan_functions.vkAllocateMemory = vkAllocateMemory;
	vulkan_functions.vkFreeMemory = vkFreeMemory;
	vulkan_functions.vkMapMemory = vkMapMemory;
	vulkan_functions.vkUnmapMemory = vkUnmapMemory;
	vulkan_functions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
	vulkan_functions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
	vulkan_functions.vkBindBufferMemory = vkBindBufferMemory;
	vulkan_functions.vkBindImageMemory = vkBindImageMemory;
	vulkan_functions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
	vulkan_functions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
	vulkan_functions.vkCreateBuffer = vkCreateBuffer;
	vulkan_functions.vkDestroyBuffer = vkDestroyBuffer;
	vulkan_functions.vkCreateImage = vkCreateImage;
	vulkan_functions.vkDestroyImage = vkDestroyImage;
	vulkan_functions.vkCmdCopyBuffer = vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
	vulkan_functions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
	vulkan_functions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
	vulkan_functions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
	vulkan_functions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
	vulkan_functions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
#endif
#if VMA_VULKAN_VERSION >= 1003000
	vulkan_functions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
	vulkan_functions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif

	VmaAllocatorCreateInfo allocator_ci = {};
	allocator_ci.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	allocator_ci.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

	allocator_ci.vulkanApiVersion = VK_API_VERSION_1_3;
	allocator_ci.physicalDevice = context().physical_device;
	allocator_ci.device = context().device;
	allocator_ci.instance = context().instance;
	allocator_ci.pVulkanFunctions = &vulkan_functions;

	vmaCreateAllocator(&allocator_ci, &context().allocator);
}

static void create_surface() {
	check(glfwCreateWindowSurface(context().instance, Window::get()->window_handle, nullptr, &context().surface),
		  "Failed to create window surface");
}

static void pick_physical_device() {
	u32 device_cnt = 0;
	vkEnumeratePhysicalDevices(context().instance, &device_cnt, nullptr);
	if (device_cnt == 0) {
		LUMEN_ERROR("Failed to find GPUs with Vulkan support");
	}

	lm::SmallArray<VkPhysicalDevice, MAX_PHYSICAL_DEVICES> devices;
	devices.resize(device_cnt);
	vkEnumeratePhysicalDevices(context().instance, &device_cnt, devices.data);

	// Is device suitable?
	auto is_suitable = [](VkPhysicalDevice device) {
		QueueFamilyIndices indices = find_queue_families(device);

		// Check device extension support
		auto extensions_supported = [](VkPhysicalDevice device) {
			u32 extension_cnt;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_cnt, nullptr);

			lm::SmallArray<VkExtensionProperties, MAX_EXTENSION_PROPERTIES> available_extensions;
			// Cap at our internal limit to avoid stack overflows, but warn if we are missing some
			if (extension_cnt > MAX_EXTENSION_PROPERTIES) {
				LUMEN_WARN("Device has %d extensions, but we only check the first %d", extension_cnt,
						   MAX_EXTENSION_PROPERTIES);
				extension_cnt = MAX_EXTENSION_PROPERTIES;
			}
			available_extensions.resize(extension_cnt);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_cnt, available_extensions.data);

			std::unordered_set<std::string> required_extensions(_device_extensions.begin(), _device_extensions.end());

			for (const auto& extension : available_extensions) {
				required_extensions.erase(extension.extensionName);
			}
			return required_extensions.empty();
		}(device);

		// Query swaphcain support
		bool swapchain_adequate = false;
		if (extensions_supported) {
			SwapChainSupportDetails swapchain_support = query_swapchain_support(device);
			// If we have a format and present mode, it's adequate
			swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
		}
		// If we have the appropiate queue families, extensions and adequate
		// swapchain, return true
		return indices.is_complete() && extensions_supported && swapchain_adequate;
	};
	for (const auto& device : devices) {
		if (is_suitable(device)) {
			context().physical_device = device;
			VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
			vkGetPhysicalDeviceFeatures2(context().physical_device, &features2);
			context().supported_features = features2.features;
			get_physical_device_properties(context().physical_device, &context().device_properties);
			get_physical_device_memory_properties(context().physical_device, &context().memory_properties);
			break;
		}
	}

	if (context().physical_device == VK_NULL_HANDLE) {
		LUMEN_ERROR("Failed to find a suitable GPU");
	}
	VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};

	VkPhysicalDeviceSubgroupProperties subgroup_props{};
	subgroup_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroup_props.pNext = &context().rt_props;

	prop2.pNext = &subgroup_props;
	vkGetPhysicalDeviceProperties2(context().physical_device, &prop2);
	if (subgroup_props.subgroupSize != 32) {
		LUMEN_WARN("Subgroup size is not 32. This may affect behavior");
	}
}

static void create_logical_device() {
	context().queue_indices = find_queue_families(context().physical_device);

	lm::SmallArray<VkDeviceQueueCreateInfo, MAX_DEVICE_QUEUES> queue_CIs;
	std::unordered_set<u32> unique_queue_families = {context().queue_indices.gfx_family.value(),
													 context().queue_indices.present_family.value(),
													 context().queue_indices.compute_family.value()};

	context().queues.resize(context().queue_indices.gfx_family.has_value() +
							context().queue_indices.present_family.has_value() +
							context().queue_indices.compute_family.has_value());
	f32 queue_priority = 1.0f;
	for (u32 queue_family_idx : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_CI{};
		queue_CI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_CI.queueFamilyIndex = queue_family_idx;
		queue_CI.queueCount = 1;
		queue_CI.pQueuePriorities = &queue_priority;
		queue_CIs.push_back(queue_CI);
	}
	// TODO: Pass these externally
	VkPhysicalDeviceFeatures2 device_features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	VkPhysicalDeviceVulkan12Features features12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_fts{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_fts{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
	VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomic_fts{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};

	VkPhysicalDeviceVulkan13Features features13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	features13.dynamicRendering = true;
	features13.synchronization2 = true;
	features13.maintenance4 = true;

	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
	VkPhysicalDeviceSynchronization2FeaturesKHR syncronization2_features = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR};
	VkPhysicalDeviceMaintenance4FeaturesKHR maintenance4_fts = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR};

	VkPhysicalDeviceRobustness2FeaturesKHR robustness2_fts = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_KHR};

	VkPhysicalDeviceRayQueryFeaturesKHR ray_query_fts = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
	ray_query_fts.rayQuery = true;
	ray_query_fts.pNext = nullptr;

	robustness2_fts.nullDescriptor = true;
	robustness2_fts.pNext = &ray_query_fts;

	atomic_fts.shaderBufferFloat32AtomicAdd = true;
	atomic_fts.shaderBufferFloat32Atomics = true;
	atomic_fts.shaderSharedFloat32AtomicAdd = true;
	atomic_fts.shaderSharedFloat32Atomics = true;
	atomic_fts.pNext = &robustness2_fts;
	accel_fts.accelerationStructure = true;
	accel_fts.descriptorBindingAccelerationStructureUpdateAfterBind = true;
	accel_fts.pNext = &atomic_fts;
	rt_fts.rayTracingPipeline = true;
	rt_fts.pNext = &accel_fts;
	features12.bufferDeviceAddress = true;
	features12.runtimeDescriptorArray = true;
	features12.shaderSampledImageArrayNonUniformIndexing = true;
	features12.scalarBlockLayout = true;
	features12.hostQueryReset = true;
	if (1) {
		dynamic_rendering_feature.dynamicRendering = true;
		syncronization2_features.synchronization2 = true;
		maintenance4_fts.maintenance4 = true;
		features12.pNext = &maintenance4_fts;
		maintenance4_fts.pNext = &syncronization2_features;
		syncronization2_features.pNext = &dynamic_rendering_feature;
		dynamic_rendering_feature.pNext = &rt_fts;
	} else {
		features12.pNext = &features13;
		features13.pNext = &rt_fts;
	}

	device_features2.features.samplerAnisotropy = true;
	device_features2.features.shaderInt64 = true;
	//
	device_features2.features.fragmentStoresAndAtomics = true;
	device_features2.features.vertexPipelineStoresAndAtomics = true;

	device_features2.pNext = &features12;

	VkDeviceCreateInfo logical_device_CI{};
	logical_device_CI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	logical_device_CI.queueCreateInfoCount = static_cast<u32>(queue_CIs.size);
	logical_device_CI.pQueueCreateInfos = queue_CIs.data;

	logical_device_CI.enabledExtensionCount = static_cast<u32>(_device_extensions.size);
	logical_device_CI.ppEnabledExtensionNames = _device_extensions.data;

	logical_device_CI.pNext = &device_features2;

	check(vkCreateDevice(context().physical_device, &logical_device_CI, nullptr, &context().device),
		  "Failed to create logical device");

	vkGetDeviceQueue(context().device, context().queue_indices.gfx_family.value(), 0,
					 &context().queues[(i32)QueueType::GFX]);
	vkGetDeviceQueue(context().device, context().queue_indices.compute_family.value(), 0,
					 &context().queues[(i32)QueueType::COMPUTE]);
	vkGetDeviceQueue(context().device, context().queue_indices.present_family.value(), 0,
					 &context().queues[(i32)QueueType::PRESENT]);
}

static void create_swapchain(VkSwapchainKHR old_swapchain = VK_NULL_HANDLE) {
	SwapChainSupportDetails swapchain_support = query_swapchain_support(context().physical_device);

	// Pick surface format, present mode and extent(preferrably width and
	// height):
	VkSurfaceFormatKHR surface_format =
		[](const lm::SmallArray<VkSurfaceFormatKHR, MAX_SURFACE_FORMATS>& available_formats) {
			for (const auto& available_format : available_formats) {
				// Preferrably SRGB32 for now
				if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
					available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					return available_format;
				}
			}
			return available_formats[0];
		}(swapchain_support.formats);

	VkPresentModeKHR present_mode = [](const lm::SmallArray<VkPresentModeKHR, MAX_PRESENT_MODES>& present_modes) {
		for (const auto& available_present_mode : present_modes) {
			// For now we prefer Mailbox
			if (available_present_mode ==
				(context().vsync_enabled ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR)) {
				return available_present_mode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}(swapchain_support.present_modes);

	// Choose swap chain extent
	VkExtent2D extent = [](const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != UINT32_MAX) {
			return capabilities.currentExtent;
		} else {
			i32 width, height;
			glfwGetFramebufferSize(Window::get()->window_handle, &width, &height);

			VkExtent2D actual_extent = {static_cast<u32>(width), static_cast<u32>(height)};

			// Clamp width and height
			actual_extent.width = glm::max(capabilities.minImageExtent.width,
										   glm::min(capabilities.maxImageExtent.width, actual_extent.width));

			actual_extent.height = glm::max(capabilities.minImageExtent.height,
											glm::min(capabilities.maxImageExtent.height, actual_extent.height));

			return actual_extent;
		}
	}(swapchain_support.capabilities);

	u32 image_cnt = swapchain_support.capabilities.minImageCount + 1;
	if (swapchain_support.capabilities.maxImageCount > 0 && image_cnt > swapchain_support.capabilities.maxImageCount) {
		image_cnt = swapchain_support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_CI{};
	swapchain_CI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_CI.surface = context().surface;

	swapchain_CI.minImageCount = image_cnt;
	swapchain_CI.imageFormat = surface_format.format;
	swapchain_CI.imageColorSpace = surface_format.colorSpace;
	swapchain_CI.imageExtent = extent;
	swapchain_CI.imageArrayLayers = 1;
	swapchain_CI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	_swapchain_format = surface_format.format;

	QueueFamilyIndices indices = find_queue_families(context().physical_device);
	u32 queue_family_indices_arr[] = {indices.gfx_family.value(), indices.present_family.value()};

	if (indices.gfx_family != indices.present_family) {
		swapchain_CI.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_CI.queueFamilyIndexCount = 2;
		swapchain_CI.pQueueFamilyIndices = queue_family_indices_arr;
	} else {
		swapchain_CI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	swapchain_CI.preTransform = swapchain_support.capabilities.currentTransform;
	swapchain_CI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_CI.presentMode = present_mode;
	swapchain_CI.clipped = VK_TRUE;

	swapchain_CI.oldSwapchain = old_swapchain;

	check(vkCreateSwapchainKHR(context().device, &swapchain_CI, nullptr, &context().swapchain),
		  "Failed to create swap chain!");

	lm::SmallArray<VkImage, MAX_SWAPCHAIN_IMAGES> images;
	vkGetSwapchainImagesKHR(context().device, context().swapchain, &image_cnt, nullptr);
	// No reserve in SmallArray, just resize/push
	images.resize(image_cnt);
	vkGetSwapchainImagesKHR(context().device, context().swapchain, &image_cnt, images.data);
	for (u32 i = 0; i < image_cnt; i++) {
		lm::String tex_name =
			lm::str_concat(_rg.arena(), "Swapchain Image #", lm::str_from_u64(_rg.arena(), i), /*cstr=*/true);
		_swapchain_images.push_back(prm::get_texture({
			.name = tex_name,
			.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.dimensions = {extent.width, extent.height, 1},
			.format = surface_format.format,
			.image = images[i],
		}));
	}
}

static void create_command_pools() {
	QueueFamilyIndices queue_family_idxs = find_queue_families(context().physical_device);
	VkCommandPoolCreateInfo pool_info = command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	pool_info.queueFamilyIndex = queue_family_idxs.gfx_family.value();
	constexpr auto MAX_COMMAND_POOL_THREAD_COUNT = 64;
	context().cmd_pools.resize(MAX_COMMAND_POOL_THREAD_COUNT);
	for (u32 i = 0; i < MAX_COMMAND_POOL_THREAD_COUNT; i++) {
		check(vkCreateCommandPool(context().device, &pool_info, nullptr, &context().cmd_pools[i]),
			  "Failed to create command pool!");
	}
}

static void create_command_buffers() {
	context().command_buffers.resize(_swapchain_images.size);
	// TODO: Factor
	// 0 is for the main thread
	VkCommandBufferAllocateInfo alloc_info = command_buffer_allocate_info(
		context().cmd_pools[0], VK_COMMAND_BUFFER_LEVEL_PRIMARY, (u32)context().command_buffers.size);
	check(vkAllocateCommandBuffers(context().device, &alloc_info, context().command_buffers.data),
		  "Failed to allocate command buffers!");
}

static void create_sync_primitives() {
	_image_available_sem.resize(MAX_FRAMES_IN_FLIGHT);
	_render_finished_sem.resize(_swapchain_images.size);
	_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
	_images_in_flight.resize(_swapchain_images.size);
	// Initialize fence array elements to NULL
	for (u64 i = 0; i < _images_in_flight.size; ++i) {
		_images_in_flight[i] = VK_NULL_HANDLE;
	}

	VkSemaphoreCreateInfo semaphore_info = semaphore();

	VkFenceCreateInfo fence_info = fence(VK_FENCE_CREATE_SIGNALED_BIT);

	for (u64 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		check(vkCreateSemaphore(context().device, &semaphore_info, nullptr, &_image_available_sem[i]),
			  "Failed to create synchronization primitives for a frame");
		check(vkCreateFence(context().device, &fence_info, nullptr, &_in_flight_fences[i]),
			  "Failed to create synchronization primitives for a frame");
	}
	for (u64 i = 0; i < _swapchain_images.size; i++) {
		check(vkCreateSemaphore(context().device, &semaphore_info, nullptr, &_render_finished_sem[i]));
	}
}

static bool check_validation_layer_support() {
	u32 layer_cnt;
	vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

	lm::SmallArray<VkLayerProperties, MAX_LAYER_PROPERTIES> available_layers;
	available_layers.resize(layer_cnt);
	vkEnumerateInstanceLayerProperties(&layer_cnt, available_layers.data);

	for (const char* layer_name : _validation_layers_lst) {
		bool layer_found = false;

		for (const auto& layerProperties : available_layers) {
			if (strcmp(layer_name, layerProperties.layerName) == 0) {
				layer_found = true;
				break;
			}
		}
		if (!layer_found) {
			return false;
		}
	}
	return true;
}

static void create_instance() {
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Lumen";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 1);
	app_info.pEngineName = "Lumen Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 1);
	app_info.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instance_CI{};
	instance_CI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_CI.pApplicationInfo = &app_info;

	auto extensions = get_req_extensions();
	instance_CI.enabledExtensionCount = static_cast<u32>(extensions.size);
	instance_CI.ppEnabledExtensionNames = extensions.data;

	if (_enable_validation_layers) {
		instance_CI.enabledLayerCount = static_cast<u32>(_validation_layers_lst.size);
		instance_CI.ppEnabledLayerNames = _validation_layers_lst.data;
		VkDebugUtilsMessengerCreateInfoEXT debug_CI = debug_messenger(debug_callback);
		instance_CI.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_CI;
	} else {
		instance_CI.enabledLayerCount = 0;
		instance_CI.pNext = nullptr;
	}
	check(volkInitialize(), "Failed to initialize volk");
	check(vkCreateInstance(&instance_CI, nullptr, &context().instance), "Failed to create instance");
	volkLoadInstance(context().instance);
	vkGetPhysicalDeviceProperties = get_physical_device_properties;
	if (_enable_validation_layers && !check_validation_layer_support()) {
		LUMEN_ERROR("Validation layers requested, but not available!");
	}
	_rg.init();
	if (_enable_validation_layers) {
		setup_debug_messenger();
	}
}

static VkQueryPool create_query_pool(VkQueryType query_type, u32 count) {
	VkQueryPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	create_info.queryType = query_type;
	create_info.queryCount = count;

	VkQueryPool query_pool;
	vk::check(vkCreateQueryPool(context().device, &create_info, 0, &query_pool));
	return query_pool;
}

void init(bool validation_layers) {
	_enable_validation_layers = validation_layers;

	// Initialize global lists
	_validation_layers_lst.push_back("VK_LAYER_KHRONOS_validation");
	_device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	create_instance();
	create_surface();
	pick_physical_device();
	create_logical_device();
	create_allocator();
	create_swapchain();
	create_command_pools();
	create_command_buffers();
	create_sync_primitives();
	init_imgui();
	context().query_pool_timestamps[0] = create_query_pool(VK_QUERY_TYPE_TIMESTAMP, 4096);
	context().query_pool_timestamps[1] = create_query_pool(VK_QUERY_TYPE_TIMESTAMP, 4096);
	context().query_pool_timestamps[2] = create_query_pool(VK_QUERY_TYPE_TIMESTAMP, 4096);
	vkResetQueryPool(context().device, context().query_pool_timestamps[0], 0, 4096);
	vkResetQueryPool(context().device, context().query_pool_timestamps[1], 0, 4096);
	vkResetQueryPool(context().device, context().query_pool_timestamps[2], 0, 4096);
}

void init_imgui() {
	VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (u32)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	check(vkCreateDescriptorPool(context().device, &pool_info, nullptr, &_imgui_pool));
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Setup Platform/Renderer backends
	ImGui::StyleColorsDark();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForVulkan(Window::get()->window_handle, true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = context().instance;
	init_info.PhysicalDevice = context().physical_device;
	init_info.Device = context().device;
	init_info.Queue = context().queues[(i32)QueueType::GFX];
	init_info.DescriptorPool = _imgui_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.UseDynamicRendering = true;
	init_info.ColorAttachmentFormat = _swapchain_format;

	ImGui_ImplVulkan_Init(&init_info, nullptr);

	CommandBuffer cmd(true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(context().queues[(i32)QueueType::GFX]);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void destroy_imgui() {
	vkDestroyDescriptorPool(context().device, _imgui_pool, nullptr);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void add_device_extension(const char* name) { _device_extensions.push_back(name); }

// Called after window resize or manually
void recreate_swap_chain() {
	i32 width = 0, height = 0;
	glfwGetFramebufferSize(Window::get()->window_handle, &width, &height);
	while (width == 0 || height == 0) {
		// Window is minimized
		glfwGetFramebufferSize(Window::get()->window_handle, &width, &height);
		glfwWaitEvents();
	}
	check(vkDeviceWaitIdle(context().device), "Failed to wait for device before recreating swap chain");
	cleanup_swapchain_images();
	VkSwapchainKHR old_swapchain = context().swapchain;
	create_swapchain(old_swapchain);
	vkDestroySwapchainKHR(context().device, old_swapchain, nullptr);
}

lm::SmallArray<Texture*, MAX_SWAPCHAIN_IMAGES>& swapchain_images() { return _swapchain_images; }

u32 prepare_frame() {
	check(vkWaitForFences(context().device, 1, &_in_flight_fences[context().in_flight_frame_idx], VK_TRUE, ~0ull),
		  "Timeout");

	u32 image_idx;
	VkResult result =
		vkAcquireNextImageKHR(context().device, context().swapchain, UINT64_MAX,
							  _image_available_sem[context().in_flight_frame_idx], VK_NULL_HANDLE, &image_idx);
	if (result == VK_NOT_READY || result == VK_TIMEOUT || result == VK_SUBOPTIMAL_KHR) {
		return UINT32_MAX;
	}
	if (_images_in_flight[image_idx] != VK_NULL_HANDLE) {
		vkWaitForFences(context().device, 1, &_images_in_flight[image_idx], VK_TRUE, UINT64_MAX);
	}

	vkResetFences(context().device, 1, &_in_flight_fences[context().in_flight_frame_idx]);
	_images_in_flight[image_idx] = _in_flight_fences[context().in_flight_frame_idx];
	check(vkResetCommandBuffer(context().command_buffers[image_idx], 0));
	GPUQueryManager::collect(u32(context().in_flight_frame_idx));
	return image_idx;
}

VkResult submit_frame(u32 image_idx) {
	VkSubmitInfo submit_info = vk::submit_info();
	VkSemaphore wait_semaphores[] = {_image_available_sem[context().in_flight_frame_idx]};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &context().command_buffers[image_idx];

	VkSemaphore signal_semaphores[] = {_render_finished_sem[image_idx]};
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	check(vkQueueSubmit(context().queues[(i32)QueueType::GFX], 1, &submit_info,
						_in_flight_fences[context().in_flight_frame_idx]),
		  "Failed to submit draw command buffer");
	context().in_flight_frame_idx = (context().in_flight_frame_idx + 1) % MAX_FRAMES_IN_FLIGHT;
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swapchains[] = {context().swapchain};
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;

	present_info.pImageIndices = &image_idx;

	VkResult result = vkQueuePresentKHR(context().queues[(i32)QueueType::GFX], &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreate_swap_chain();
		return result;
	} else if (result != VK_SUCCESS) {
		LUMEN_ERROR("Failed to present swap chain image");
	}

	return result;
}

lm::RenderGraph* render_graph() { return &_rg; }

void cleanup_app_data() { _rg.destroy(); }

void cleanup() {
	cleanup_app_data();
	cleanup_swapchain_images();
	vkDestroyQueryPool(context().device, context().query_pool_timestamps[0], nullptr);
	vkDestroyQueryPool(context().device, context().query_pool_timestamps[1], nullptr);
	vkDestroyQueryPool(context().device, context().query_pool_timestamps[2], nullptr);
	vkDestroySwapchainKHR(context().device, context().swapchain, nullptr);
	vk::event_pool::cleanup();
	vkFreeCommandBuffers(context().device, context().cmd_pools[0], static_cast<u32>(context().command_buffers.size),
						 context().command_buffers.data);
	for (u64 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(context().device, _image_available_sem[i], nullptr);
		vkDestroyFence(context().device, _in_flight_fences[i], nullptr);
	}
	for (VkSemaphore sem : _render_finished_sem) {
		vkDestroySemaphore(context().device, sem, nullptr);
	}

	for (auto pool : context().cmd_pools) {
		vkDestroyCommandPool(context().device, pool, nullptr);
	}
	vkDestroySurfaceKHR(context().instance, context().surface, nullptr);
	prm::destroy();
	vmaDestroyAllocator(context().allocator);

	vkDestroyDevice(context().device, nullptr);
	if (_enable_validation_layers) {
		vkExt_destroy_debug_messenger(context().instance, context().debug_messenger, nullptr);
	}
	vkDestroyInstance(context().instance, nullptr);
}

}  // namespace vk
