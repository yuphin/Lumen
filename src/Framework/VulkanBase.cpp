#include <vulkan/vulkan_core.h>
#include "Framework/VulkanContext.h"
#include "LumenPCH.h"
#include "RenderGraph.h"
#include "VulkanContext.h"
#define VOLK_IMPLEMENTATION
#include "VulkanBase.h"
#include "CommandBuffer.h"
#include "PersistentResourceManager.h"

namespace lumen {

namespace VulkanBase {

constexpr int MAX_FRAMES_IN_FLIGHT = 3;

const std::vector<const char*> _validation_layers_lst = {"VK_LAYER_KHRONOS_validation"};

std::vector<const char*> _device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

size_t current_frame = 0;
// Sync primitives
std::vector<VkSemaphore> _image_available_sem;
std::vector<VkSemaphore> _render_finished_sem;
std::vector<VkFence> _in_flight_fences;
std::vector<VkFence> _images_in_flight;
std::vector<VkQueueFamilyProperties> _queue_families;
std::unique_ptr<RenderGraph> _rg;
VkFormat _swapchain_format;

std::vector<vk::Texture*> _swapchain_images;

bool _enable_validation_layers;

VkDescriptorPool _imgui_pool = 0;

std::vector<const char*> get_req_extensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (_enable_validation_layers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	return extensions;
}

vk::QueueFamilyIndices find_queue_families(VkPhysicalDevice device) {
	vk::QueueFamilyIndices indices;
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	_queue_families.resize(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, _queue_families.data());

	int i = 0;
	for (const auto& queueFamily : _queue_families) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.gfx_family = i;
		}

		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
			indices.compute_family = i;
		}

		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vk::context().surface, &present_support);

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

SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device) {
	// Basically returns present modes and surface modes in a struct

	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vk::context().surface, &details.capabilities);

	uint32_t format_cnt;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, vk::context().surface, &format_cnt, nullptr);

	if (format_cnt != 0) {
		details.formats.resize(format_cnt);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, vk::context().surface, &format_cnt, details.formats.data());
	}

	uint32_t present_mode_cnt;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, vk::context().surface, &present_mode_cnt, nullptr);

	if (present_mode_cnt != 0) {
		details.present_modes.resize(present_mode_cnt);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, vk::context().surface, &present_mode_cnt,
												  details.present_modes.data());
	}
	return details;
}

VkResult vkExt_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
									  const VkAllocationCallbacks* pAllocator,
									  VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void vkExt_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
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
	// if ((messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	//	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) ==
	//	0) {
	//	return VK_TRUE;
	// }

	if ((messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) == 0) {
		// LUMEN_TRACE("Validation Warning: {0} ", pCallbackData->pMessage);
		return VK_TRUE;
	}
	LUMEN_ERROR("Validation Error: {0} ", pCallbackData->pMessage);
	return VK_FALSE;
}

static void setup_debug_messenger() {
	auto ci = vk::debug_messenger(debug_callback);

	vk::check(vkExt_create_debug_messenger(vk::context().instance, &ci, nullptr, &vk::context().debug_messenger),
			  "Failed to set up debug messenger!");
}
void cleanup_swapchain() {
	// Order:
	// 1- Destroy Framebuffers
	// 2- Destroy Commandbuffers
	// 3- Destroy the pipelines
	// 4- Destroy pipeline layout
	// 5- Destroy render pass
	// 6- Destroy image views
	// 7- Destroy swapchain
	// TODO
	// for (auto framebuffer : vk::context().swapchain_framebuffers) {
	//	vkDestroyFramebuffer(vk::context().device, framebuffer, nullptr);
	//}
	vkDestroyDescriptorPool(vk::context().device, _imgui_pool, nullptr);
	vkFreeCommandBuffers(vk::context().device, vk::context().cmd_pools[0],
						 static_cast<uint32_t>(vk::context().command_buffers.size()),
						 vk::context().command_buffers.data());
	for (vk::Texture* swapchain_img : _swapchain_images) {
		prm::remove(swapchain_img);
	}
	_swapchain_images.clear();
	vkDestroySwapchainKHR(vk::context().device, vk::context().swapchain, nullptr);
}

void cleanup_app_data() { _rg->destroy(); }

void cleanup() {
	cleanup_app_data();
	cleanup_swapchain();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(vk::context().device, _image_available_sem[i], nullptr);
		vkDestroySemaphore(vk::context().device, _render_finished_sem[i], nullptr);
		vkDestroyFence(vk::context().device, _in_flight_fences[i], nullptr);
	}

	for (auto pool : vk::context().cmd_pools) {
		vkDestroyCommandPool(vk::context().device, pool, nullptr);
	}
	vkDestroySurfaceKHR(vk::context().instance, vk::context().surface, nullptr);
	vmaDestroyAllocator(vk::context().allocator);

	vkDestroyDevice(vk::context().device, nullptr);
	if (_enable_validation_layers) {
		vkExt_destroy_debug_messenger(vk::context().instance, vk::context().debug_messenger, nullptr);
	}
	vkDestroyInstance(vk::context().instance, nullptr);
	glfwDestroyWindow(vk::context().window_ptr);
	glfwTerminate();
}

static void create_allocator() {
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
	vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
	vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
	vulkanFunctions.vkFreeMemory = vkFreeMemory;
	vulkanFunctions.vkMapMemory = vkMapMemory;
	vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
	vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
	vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
	vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
	vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
	vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
	vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
	vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
	vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
	vulkanFunctions.vkCreateImage = vkCreateImage;
	vulkanFunctions.vkDestroyImage = vkDestroyImage;
	vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
	vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
	vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
	vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
	vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
	vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
#endif
#if VMA_VULKAN_VERSION >= 1003000
	vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
	vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	// allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorCreateInfo.physicalDevice = vk::context().physical_device;
	allocatorCreateInfo.device = vk::context().device;
	allocatorCreateInfo.instance = vk::context().instance;
	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

	vmaCreateAllocator(&allocatorCreateInfo, &vk::context().allocator);
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
	instance_CI.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instance_CI.ppEnabledExtensionNames = extensions.data();

	if (_enable_validation_layers) {
		instance_CI.enabledLayerCount = static_cast<uint32_t>(_validation_layers_lst.size());
		instance_CI.ppEnabledLayerNames = _validation_layers_lst.data();
		VkDebugUtilsMessengerCreateInfoEXT debug_CI = vk::debug_messenger(debug_callback);
		instance_CI.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_CI;
	} else {
		instance_CI.enabledLayerCount = 0;
		instance_CI.pNext = nullptr;
	}
	vk::check(volkInitialize(), "Failed to initialize volk");
	vk::check(vkCreateInstance(&instance_CI, nullptr, &vk::context().instance), "Failed to create instance");
	volkLoadInstance(vk::context().instance);
	if (_enable_validation_layers && !check_validation_layer_support()) {
		LUMEN_ERROR("Validation layers requested, but not available!");
	}
	_rg = std::make_unique<RenderGraph>();
	if (lumen::VulkanBase::_enable_validation_layers) {
		lumen::VulkanBase::setup_debug_messenger();
	}
}

static void create_surface() {
	vk::check(
		glfwCreateWindowSurface(vk::context().instance, vk::context().window_ptr, nullptr, &vk::context().surface),
		"Failed to create window surface");
}

static void pick_physical_device() {
	uint32_t device_cnt = 0;
	vkEnumeratePhysicalDevices(vk::context().instance, &device_cnt, nullptr);
	if (device_cnt == 0) {
		LUMEN_ERROR("Failed to find GPUs with Vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(device_cnt);
	vkEnumeratePhysicalDevices(vk::context().instance, &device_cnt, devices.data());

	// Is device suitable?
	auto is_suitable = [](VkPhysicalDevice device) {
		vk::QueueFamilyIndices indices = find_queue_families(device);

		// Check device extension support
		auto extensions_supported = [](VkPhysicalDevice device) {
			uint32_t extension_cnt;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_cnt, nullptr);

			std::vector<VkExtensionProperties> available_extensions(extension_cnt);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_cnt, available_extensions.data());

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
			vk::context().physical_device = device;
			vkGetPhysicalDeviceFeatures(vk::context().physical_device, &vk::context().supported_features);
			vkGetPhysicalDeviceProperties(vk::context().physical_device, &vk::context().device_properties);
			vkGetPhysicalDeviceMemoryProperties(vk::context().physical_device, &vk::context().memory_properties);
			break;
		}
	}

	if (vk::context().physical_device == VK_NULL_HANDLE) {
		LUMEN_ERROR("Failed to find a suitable GPU");
	}
	VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	prop2.pNext = &vk::context().rt_props;
	vkGetPhysicalDeviceProperties2(vk::context().physical_device, &prop2);
}

static void create_logical_device() {
	vk::context().queue_indices = find_queue_families(vk::context().physical_device);

	std::vector<VkDeviceQueueCreateInfo> queue_CIs;
	std::unordered_set<uint32_t> unique_queue_families = {vk::context().queue_indices.gfx_family.value(),
														  vk::context().queue_indices.present_family.value(),
														  vk::context().queue_indices.compute_family.value()};

	vk::context().queues.resize(vk::context().queue_indices.gfx_family.has_value() +
								vk::context().queue_indices.present_family.has_value() +
								vk::context().queue_indices.compute_family.has_value());
	float queue_priority = 1.0f;
	for (uint32_t queue_family_idx : unique_queue_families) {
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

	atomic_fts.shaderBufferFloat32AtomicAdd = true;
	atomic_fts.shaderBufferFloat32Atomics = true;
	atomic_fts.shaderSharedFloat32AtomicAdd = true;
	atomic_fts.shaderSharedFloat32Atomics = true;
	atomic_fts.pNext = nullptr;
	accel_fts.accelerationStructure = true;
	accel_fts.pNext = &atomic_fts;
	rt_fts.rayTracingPipeline = true;
	rt_fts.pNext = &accel_fts;
	features12.bufferDeviceAddress = true;
	features12.runtimeDescriptorArray = true;
	features12.shaderSampledImageArrayNonUniformIndexing = true;
	features12.scalarBlockLayout = true;
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
	//

	device_features2.pNext = &features12;

	VkDeviceCreateInfo logical_device_CI{};
	logical_device_CI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	logical_device_CI.queueCreateInfoCount = static_cast<uint32_t>(queue_CIs.size());
	logical_device_CI.pQueueCreateInfos = queue_CIs.data();

	logical_device_CI.enabledExtensionCount = static_cast<uint32_t>(_device_extensions.size());
	logical_device_CI.ppEnabledExtensionNames = _device_extensions.data();

	logical_device_CI.pNext = &device_features2;

	if (_enable_validation_layers) {
		logical_device_CI.enabledLayerCount = static_cast<uint32_t>(_validation_layers_lst.size());
		logical_device_CI.ppEnabledLayerNames = _validation_layers_lst.data();
	} else {
		logical_device_CI.enabledLayerCount = 0;
	}

	vk::check(vkCreateDevice(vk::context().physical_device, &logical_device_CI, nullptr, &vk::context().device),
			  "Failed to create logical device");

	// load_VK_EXTENSIONS(vk::context().instance, vkGetInstanceProcAddr, vk::context().device, vkGetDeviceProcAddr);
	vkGetDeviceQueue(vk::context().device, vk::context().queue_indices.gfx_family.value(), 0,
					 &vk::context().queues[(int)vk::QueueType::GFX]);
	vkGetDeviceQueue(vk::context().device, vk::context().queue_indices.compute_family.value(), 0,
					 &vk::context().queues[(int)vk::QueueType::COMPUTE]);
	vkGetDeviceQueue(vk::context().device, vk::context().queue_indices.present_family.value(), 0,
					 &vk::context().queues[(int)vk::QueueType::PRESENT]);
}

static void create_swapchain() {
	SwapChainSupportDetails swapchain_support = query_swapchain_support(vk::context().physical_device);

	// Pick surface format, present mode and extent(preferrably width and
	// height):

	VkSurfaceFormatKHR surface_format = [](const std::vector<VkSurfaceFormatKHR>& available_formats) {
		for (const auto& available_format : available_formats) {
			// Preferrably SRGB32 for now
			if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
				available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return available_format;
			}
		}
		return available_formats[0];
	}(swapchain_support.formats);

	VkPresentModeKHR present_mode = [](const std::vector<VkPresentModeKHR>& present_modes) {
		for (const auto& available_present_mode : present_modes) {
			// For now we prefer Mailbox
			if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
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
			int width, height;
			glfwGetFramebufferSize(vk::context().window_ptr, &width, &height);

			VkExtent2D actual_extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

			// Clamp width and height
			actual_extent.width = std::max(capabilities.minImageExtent.width,
										   std::min(capabilities.maxImageExtent.width, actual_extent.width));

			actual_extent.height = std::max(capabilities.minImageExtent.height,
											std::min(capabilities.maxImageExtent.height, actual_extent.height));

			return actual_extent;
		}
	}(swapchain_support.capabilities);

	uint32_t image_cnt = swapchain_support.capabilities.minImageCount + 1;
	if (swapchain_support.capabilities.maxImageCount > 0 && image_cnt > swapchain_support.capabilities.maxImageCount) {
		image_cnt = swapchain_support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_CI{};
	swapchain_CI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_CI.surface = vk::context().surface;

	swapchain_CI.minImageCount = image_cnt;
	swapchain_CI.imageFormat = surface_format.format;
	swapchain_CI.imageColorSpace = surface_format.colorSpace;
	swapchain_CI.imageExtent = extent;
	swapchain_CI.imageArrayLayers = 1;
	swapchain_CI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	_swapchain_format = surface_format.format;

	vk::QueueFamilyIndices indices = find_queue_families(vk::context().physical_device);
	uint32_t queue_family_indices_arr[] = {indices.gfx_family.value(), indices.present_family.value()};

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

	swapchain_CI.oldSwapchain = VK_NULL_HANDLE;

	vk::check(vkCreateSwapchainKHR(vk::context().device, &swapchain_CI, nullptr, &vk::context().swapchain),
			  "Failed to create swap chain!");

	std::vector<VkImage> images;
	vkGetSwapchainImagesKHR(vk::context().device, vk::context().swapchain, &image_cnt, nullptr);
	_swapchain_images.reserve(image_cnt);
	images.resize(image_cnt);
	vkGetSwapchainImagesKHR(vk::context().device, vk::context().swapchain, &image_cnt, images.data());
	for (uint32_t i = 0; i < image_cnt; i++) {
		_swapchain_images.emplace_back(prm::get_texture({
			.name = "Swapchain Image #" + std::to_string(i),
			.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.dimensions = {extent.width, extent.height, 1},
			.format = surface_format.format,
			.image = images[i],
		}));
	}
}

static void create_command_pools() {
	vk::QueueFamilyIndices queue_family_idxs = find_queue_families(vk::context().physical_device);
	VkCommandPoolCreateInfo pool_info = vk::command_pool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	pool_info.queueFamilyIndex = queue_family_idxs.gfx_family.value();
	const auto processor_count = std::thread::hardware_concurrency();
	vk::context().cmd_pools.resize(processor_count);
	for (unsigned int i = 0; i < processor_count; i++) {
		vk::check(vkCreateCommandPool(vk::context().device, &pool_info, nullptr, &vk::context().cmd_pools[i]),
				  "Failed to create command pool!");
	}
}

static void create_command_buffers() {
	vk::context().command_buffers.resize(_swapchain_images.size());
	// TODO: Factor
	// 0 is for the main thread
	VkCommandBufferAllocateInfo alloc_info = vk::command_buffer_allocate_info(
		vk::context().cmd_pools[0], VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)vk::context().command_buffers.size());
	vk::check(vkAllocateCommandBuffers(vk::context().device, &alloc_info, vk::context().command_buffers.data()),
			  "Failed to allocate command buffers!");
}

static void create_sync_primitives() {
	_image_available_sem.resize(MAX_FRAMES_IN_FLIGHT);
	_render_finished_sem.resize(MAX_FRAMES_IN_FLIGHT);
	_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
	_images_in_flight.resize(_swapchain_images.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphore_info = vk::semaphore();

	VkFenceCreateInfo fence_info = vk::fence(VK_FENCE_CREATE_SIGNALED_BIT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::check<3>({vkCreateSemaphore(vk::context().device, &semaphore_info, nullptr, &_image_available_sem[i]),
					  vkCreateSemaphore(vk::context().device, &semaphore_info, nullptr, &_render_finished_sem[i]),
					  vkCreateFence(vk::context().device, &fence_info, nullptr, &_in_flight_fences[i])},
					 "Failed to create synchronization primitives for a frame");
	}
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
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	vk::check(vkCreateDescriptorPool(vk::context().device, &pool_info, nullptr, &_imgui_pool));
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Setup Platform/Renderer backends
	ImGui::StyleColorsDark();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForVulkan(vk::context().window_ptr, true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = vk::context().instance;
	init_info.PhysicalDevice = vk::context().physical_device;
	init_info.Device = vk::context().device;
	init_info.Queue = vk::context().queues[(int)vk::QueueType::GFX];
	init_info.DescriptorPool = _imgui_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.UseDynamicRendering = true;
	init_info.ColorAttachmentFormat = _swapchain_format;

	ImGui_ImplVulkan_Init(&init_info, nullptr);

	CommandBuffer cmd(true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(vk::context().queues[(int)vk::QueueType::GFX]);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void init(bool validation_layers) {
	_enable_validation_layers = validation_layers;
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
}

void destroy_imgui() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

// Called after window resize
void recreate_swap_chain() {
	int width = 0, height = 0;
	glfwGetFramebufferSize(vk::context().window_ptr, &width, &height);
	while (width == 0 || height == 0) {
		// Window is minimized
		glfwGetFramebufferSize(vk::context().window_ptr, &width, &height);
		glfwWaitEvents();
	}
	vkDeviceWaitIdle(vk::context().device);
	cleanup_swapchain();
	create_swapchain();
	create_command_buffers();
}

void add_device_extension(const char* name) { _device_extensions.push_back(name); }

bool check_validation_layer_support() {
	uint32_t layer_cnt;
	vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

	std::vector<VkLayerProperties> available_layers(layer_cnt);
	vkEnumerateInstanceLayerProperties(&layer_cnt, available_layers.data());

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

uint32_t prepare_frame() {
	vk::check(vkWaitForFences(vk::context().device, 1, &_in_flight_fences[current_frame], VK_TRUE, 1000000000),
			  "Timeout");

	uint32_t image_idx;
	VkResult result = vkAcquireNextImageKHR(vk::context().device, vk::context().swapchain, UINT64_MAX,
											_image_available_sem[current_frame], VK_NULL_HANDLE, &image_idx);
	if (result == VK_NOT_READY) {
		return UINT32_MAX;
	} else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		// Window resize
		vkDeviceWaitIdle(vk::context().device);
		recreate_swap_chain();
		cleanup_app_data();
		_rg = std::make_unique<RenderGraph>();
		return UINT32_MAX;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		LUMEN_ERROR("Failed to acquire new swap chain image");
	}
	if (_images_in_flight[image_idx] != VK_NULL_HANDLE) {
		vkWaitForFences(vk::context().device, 1, &_images_in_flight[image_idx], VK_TRUE, UINT64_MAX);
	}

	vk::check(vkResetCommandBuffer(vk::context().command_buffers[image_idx], 0));
	vkResetFences(vk::context().device, 1, &_in_flight_fences[current_frame]);
	_images_in_flight[image_idx] = _in_flight_fences[current_frame];
	return image_idx;
}

VkResult submit_frame(uint32_t image_idx) {
	VkSubmitInfo submit_info = vk::submit_info();
	VkSemaphore wait_semaphores[] = {_image_available_sem[current_frame]};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk::context().command_buffers[image_idx];

	VkSemaphore signal_semaphores[] = {_render_finished_sem[current_frame]};
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	vk::check(
		vkQueueSubmit(vk::context().queues[(int)vk::QueueType::GFX], 1, &submit_info, _in_flight_fences[current_frame]),
		"Failed to submit draw command buffer");
	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swapchains[] = {vk::context().swapchain};
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;

	present_info.pImageIndices = &image_idx;

	VkResult result = vkQueuePresentKHR(vk::context().queues[(int)vk::QueueType::GFX], &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		vkDeviceWaitIdle(vk::context().device);
		recreate_swap_chain();
		cleanup_app_data();
		_rg = std::make_unique<RenderGraph>();
		return result;
	} else if (result != VK_SUCCESS) {
		LUMEN_ERROR("Failed to present swap chain image");
	}

	return result;
}

RenderGraph* render_graph() { return _rg.get(); }

std::vector<vk::Texture*>& swapchain_images() { return _swapchain_images; }
}  // namespace VulkanBase

}  // namespace lumen
