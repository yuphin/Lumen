#include "LumenPCH.h"
#include "RenderGraph.h"
#define VOLK_IMPLEMENTATION
#include "VulkanBase.h"
#include "CommandBuffer.h"
#include "VkUtils.h"
#include <numeric>

namespace lumen {

namespace VulkanBase {

class RTAccels {
	std::vector<AccelKHR> blases;
	AccelKHR tlas;
};

constexpr int MAX_FRAMES_IN_FLIGHT = 3;

const std::vector<const char*> validation_layers_lst = {"VK_LAYER_KHRONOS_validation"};

std::vector<const char*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

size_t current_frame = 0;
// Sync primitives
std::vector<VkSemaphore> image_available_sem;
std::vector<VkSemaphore> render_finished_sem;
std::vector<VkFence> in_flight_fences;
std::vector<VkFence> images_in_flight;
std::vector<VkQueueFamilyProperties> queue_families;
std::unique_ptr<RenderGraph> rg;
VkFormat swapchain_format;

std::vector<Texture2D> _swapchain_images;

bool enable_validation_layers;

VkDescriptorPool imgui_pool = 0;

bool has_flag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }

void init(bool validation) { enable_validation_layers = validation; }

std::vector<const char*> get_req_extensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enable_validation_layers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}

vk::QueueFamilyIndices find_queue_families(VkPhysicalDevice device) {
	vk::QueueFamilyIndices indices;
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	queue_families.resize(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	int i = 0;
	for (const auto& queueFamily : queue_families) {
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

void setup_debug_messenger() {
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
	vkDestroyDescriptorPool(vk::context().device, imgui_pool, nullptr);
	vkFreeCommandBuffers(vk::context().device, vk::context().cmd_pools[0],
						 static_cast<uint32_t>(vk::context().command_buffers.size()),
						 vk::context().command_buffers.data());
	for (auto& swapchain_img : _swapchain_images) {
		swapchain_img.destroy();
	}
	_swapchain_images.clear();
	vkDestroySwapchainKHR(vk::context().device, vk::context().swapchain, nullptr);
}

void cleanup_app_data() { rg->destroy(); }

void cleanup() {
	cleanup_app_data();
	cleanup_swapchain();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(vk::context().device, image_available_sem[i], nullptr);
		vkDestroySemaphore(vk::context().device, render_finished_sem[i], nullptr);
		vkDestroyFence(vk::context().device, in_flight_fences[i], nullptr);
	}

	for (auto pool : vk::context().cmd_pools) {
		vkDestroyCommandPool(vk::context().device, pool, nullptr);
	}
	vkDestroySurfaceKHR(vk::context().instance, vk::context().surface, nullptr);

	vkDestroyDevice(vk::context().device, nullptr);
	if (enable_validation_layers) {
		vkExt_destroy_debug_messenger(vk::context().instance, vk::context().debug_messenger, nullptr);
	}
	vkDestroyInstance(vk::context().instance, nullptr);
	glfwDestroyWindow(vk::context().window_ptr);
	glfwTerminate();
}

void create_instance() {
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

	if (enable_validation_layers) {
		instance_CI.enabledLayerCount = static_cast<uint32_t>(validation_layers_lst.size());
		instance_CI.ppEnabledLayerNames = validation_layers_lst.data();
		VkDebugUtilsMessengerCreateInfoEXT debug_CI = vk::debug_messenger(debug_callback);
		instance_CI.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_CI;
	} else {
		instance_CI.enabledLayerCount = 0;
		instance_CI.pNext = nullptr;
	}
	vk::check(volkInitialize(), "Failed to initialize volk");
	vk::check(vkCreateInstance(&instance_CI, nullptr, &vk::context().instance), "Failed to create instance");
	volkLoadInstance(vk::context().instance);
	if (enable_validation_layers && !check_validation_layer_support()) {
		LUMEN_ERROR("Validation layers requested, but not available!");
	}
	rg = std::make_unique<RenderGraph>();
	if (lumen::VulkanBase::enable_validation_layers) {
		lumen::VulkanBase::setup_debug_messenger();
	}
}

void create_surface() {
	vk::check(
		glfwCreateWindowSurface(vk::context().instance, vk::context().window_ptr, nullptr, &vk::context().surface),
		"Failed to create window surface");
}

void pick_physical_device() {
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

			std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

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

void create_logical_device() {
	vk::context().queue_indices = find_queue_families(vk::context().physical_device);

	std::vector<VkDeviceQueueCreateInfo> queue_CIs;
	std::set<uint32_t> unique_queue_families = {vk::context().queue_indices.gfx_family.value(),
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

	logical_device_CI.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	logical_device_CI.ppEnabledExtensionNames = device_extensions.data();

	logical_device_CI.pNext = &device_features2;

	if (enable_validation_layers) {
		logical_device_CI.enabledLayerCount = static_cast<uint32_t>(validation_layers_lst.size());
		logical_device_CI.ppEnabledLayerNames = validation_layers_lst.data();
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

void create_swapchain() {
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

	swapchain_format = surface_format.format;

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

	vkGetSwapchainImagesKHR(vk::context().device, vk::context().swapchain, &image_cnt, nullptr);
	_swapchain_images.reserve(image_cnt);
	VkImage images[16] = {nullptr};
	vkGetSwapchainImagesKHR(vk::context().device, vk::context().swapchain, &image_cnt, images);
	for (uint32_t i = 0; i < image_cnt; i++) {
		_swapchain_images.emplace_back("Swapchain Image #" + std::to_string(i), images[i], surface_format.format,
									  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, extent, true);
	}
}

void create_command_pools() {
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
	vk::check(vkCreateDescriptorPool(vk::context().device, &pool_info, nullptr, &imgui_pool));
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
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.UseDynamicRendering = true;
	init_info.ColorAttachmentFormat = swapchain_format;

	ImGui_ImplVulkan_Init(&init_info, nullptr);

	CommandBuffer cmd(true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(vk::context().queues[(int)vk::QueueType::GFX]);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void destroy_imgui() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void create_command_buffers() {
	vk::context().command_buffers.resize(_swapchain_images.size());
	// TODO: Factor
	// 0 is for the main thread
	VkCommandBufferAllocateInfo alloc_info = vk::command_buffer_allocate_info(
		vk::context().cmd_pools[0], VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)vk::context().command_buffers.size());
	vk::check(vkAllocateCommandBuffers(vk::context().device, &alloc_info, vk::context().command_buffers.data()),
			  "Failed to allocate command buffers!");
}

void create_sync_primitives() {
	image_available_sem.resize(MAX_FRAMES_IN_FLIGHT);
	render_finished_sem.resize(MAX_FRAMES_IN_FLIGHT);
	in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
	images_in_flight.resize(_swapchain_images.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphore_info = vk::semaphore();

	VkFenceCreateInfo fence_info = vk::fence(VK_FENCE_CREATE_SIGNALED_BIT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::check<3>({vkCreateSemaphore(vk::context().device, &semaphore_info, nullptr, &image_available_sem[i]),
					  vkCreateSemaphore(vk::context().device, &semaphore_info, nullptr, &render_finished_sem[i]),
					  vkCreateFence(vk::context().device, &fence_info, nullptr, &in_flight_fences[i])},
					 "Failed to create synchronization primitives for a frame");
	}
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

void add_device_extension(const char* name) { device_extensions.push_back(name); }

bool check_validation_layer_support() {
	uint32_t layer_cnt;
	vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr);

	std::vector<VkLayerProperties> available_layers(layer_cnt);
	vkEnumerateInstanceLayerProperties(&layer_cnt, available_layers.data());

	for (const char* layer_name : validation_layers_lst) {
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

AccelKHR create_acceleration(VkAccelerationStructureCreateInfoKHR& accel) {
	AccelKHR result_accel;
	// Allocating the buffer to hold the acceleration structure
	Buffer accel_buff;
	accel_buff.create(
		"Blas Buffer",
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, accel.size);
	// Setting the buffer
	result_accel.buffer = accel_buff;
	accel.buffer = result_accel.buffer.handle;
	// Create the acceleration structure
	vkCreateAccelerationStructureKHR(vk::context().device, &accel, nullptr, &result_accel.accel);
	return result_accel;
}

void cmd_compact_blas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
					  std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool) {
	uint32_t query_cnt{0};
	std::vector<AccelKHR> cleanupAS;  // previous AS to destroy

	// Get the compacted size result back
	std::vector<VkDeviceSize> compact_sizes(static_cast<uint32_t>(indices.size()));
	vkGetQueryPoolResults(vk::context().device, queryPool, 0, (uint32_t)compact_sizes.size(),
						  compact_sizes.size() * sizeof(VkDeviceSize), compact_sizes.data(), sizeof(VkDeviceSize),
						  VK_QUERY_RESULT_WAIT_BIT);

	for (auto idx : indices) {
		buildAs[idx].cleanup_as = buildAs[idx].as;										// previous AS to destroy
		buildAs[idx].size_info.accelerationStructureSize = compact_sizes[query_cnt++];	// new reduced size
		// Creating a compact version of the AS
		VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		asCreateInfo.size = buildAs[idx].size_info.accelerationStructureSize;
		asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildAs[idx].as = create_acceleration(asCreateInfo);
		// Copy the original BLAS to a compact version
		VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
		copyInfo.src = buildAs[idx].build_info.dstAccelerationStructure;
		copyInfo.dst = buildAs[idx].as.accel;
		copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
		vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
	}
}

void cmd_create_blas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
					 std::vector<BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress,
					 VkQueryPool queryPool) {
	if (queryPool)	// For querying the compaction size
		vkResetQueryPool(vk::context().device, queryPool, 0, static_cast<uint32_t>(indices.size()));
	uint32_t query_cnt{0};
	for (const auto& idx : indices) {
		// Actual allocation of buffer and acceleration structure.
		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = buildAs[idx].size_info.accelerationStructureSize;	 // Will be used to allocate memory.
		buildAs[idx].as = create_acceleration(createInfo);
		// BuildInfo #2 part
		buildAs[idx].build_info.dstAccelerationStructure = buildAs[idx].as.accel;  // Setting where the build lands
		buildAs[idx].build_info.scratchData.deviceAddress =
			scratchAddress;	 // All build are using the same scratch buffer
		// Building the bottom-level-acceleration-structure
		vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[idx].build_info, &buildAs[idx].range_info);

		// Since the scratch buffer is reused across builds, we need a barrier
		// to ensure one build is finished before starting the next one.
		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
							 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
							 nullptr);

		if (queryPool) {
			// Add a query to find the 'real' amount of memory needed, use for
			// compaction
			vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &buildAs[idx].build_info.dstAccelerationStructure,
														  VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
														  queryPool, query_cnt++);
		}
	}
}

static void cmd_create_tlas(AccelKHR& tlas, VkCommandBuffer cmdBuf, uint32_t countInstance, Buffer& scratchBuffer,
							VkDeviceAddress instBufferAddr, VkBuildAccelerationStructureFlagsKHR flags, bool update) {
	// Wraps a device pointer to the above uploaded instances.
	VkAccelerationStructureGeometryInstancesDataKHR instances_vk{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
	instances_vk.data.deviceAddress = instBufferAddr;

	// Put the above into a VkAccelerationStructureGeometryKHR. We need to put
	// the instances struct in a union and label it as instance data.
	VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	topASGeometry.geometry.instances = instances_vk;

	// Find sizes
	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	build_info.flags = flags;
	build_info.geometryCount = 1;
	build_info.pGeometries = &topASGeometry;
	build_info.mode =
		update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	build_info.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	vkGetAccelerationStructureBuildSizesKHR(vk::context().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
											&build_info, &countInstance, &size_info);

#ifdef VK_NV_ray_tracing_motion_blur
	VkAccelerationStructureMotionInfoNV motionInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV};
	motionInfo.maxInstances = countInstance;
#endif

	// Create TLAS
	if (update == false) {
		VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		create_info.size = size_info.accelerationStructureSize;
		tlas = create_acceleration(create_info);
	}

	// Allocate the scratch memory
	scratchBuffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, size_info.buildScratchSize);
	VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer.handle};
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(vk::context().device, &bufferInfo);

	// Update build information
	build_info.srcAccelerationStructure = update ? tlas.accel : VK_NULL_HANDLE;
	build_info.dstAccelerationStructure = tlas.accel;
	build_info.scratchData.deviceAddress = scratchAddress;

	// Build Offsets info: n instances
	VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{countInstance, 0, 0, 0};
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

	// Build the TLAS
	vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &build_info, &pBuildOffsetInfo);
}

//--------------------------------------------------------------------------------------------------
// Create all the BLAS from the vector of BlasInput
// - There will be one BLAS per input-vector entry
// - There will be as many BLAS as input.size()
// - The resulting BLAS (along with the inputs used to build) are stored in
// m_blas,
//   and can be referenced by index.
// - if flag has the 'Compact' flag, the BLAS will be compacted
//
void build_blas(std::vector<AccelKHR>& blases, const std::vector<vk::BlasInput>& input,
				VkBuildAccelerationStructureFlagsKHR flags) {
	uint32_t nb_blas = static_cast<uint32_t>(input.size());
	VkDeviceSize as_total_size{0};	   // Memory size of all allocated BLAS
	uint32_t nb_compactions{0};		   // Nb of BLAS requesting compaction
	VkDeviceSize max_scratch_size{0};  // Largest scratch size

	// Preparing the information for the acceleration build commands.
	std::vector<BuildAccelerationStructure> buildAs(nb_blas);
	for (uint32_t idx = 0; idx < nb_blas; idx++) {
		// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for
		// querying the build sizes. Other information will be filled in the
		// createBlas (see #2)
		buildAs[idx].build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildAs[idx].build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildAs[idx].build_info.flags = input[idx].flags | flags;
		buildAs[idx].build_info.geometryCount = static_cast<uint32_t>(input[idx].as_geom.size());
		buildAs[idx].build_info.pGeometries = input[idx].as_geom.data();

		// Build range information
		buildAs[idx].range_info = input[idx].as_build_offset_info.data();

		// Finding sizes to create acceleration structures and scratch
		std::vector<uint32_t> maxPrimCount(input[idx].as_build_offset_info.size());
		for (auto tt = 0; tt < input[idx].as_build_offset_info.size(); tt++) {
			maxPrimCount[tt] = input[idx].as_build_offset_info[tt].primitiveCount;	// Number of primitives/triangles
		}
		vkGetAccelerationStructureBuildSizesKHR(vk::context().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&buildAs[idx].build_info, maxPrimCount.data(), &buildAs[idx].size_info);

		// Extra info
		as_total_size += buildAs[idx].size_info.accelerationStructureSize;
		max_scratch_size = std::max(max_scratch_size, buildAs[idx].size_info.buildScratchSize);
		nb_compactions +=
			has_flag(buildAs[idx].build_info.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
	}

	// Allocate the scratch buffers holding the temporary data of the
	// acceleration structure builder
	Buffer scratch_buffer;
	scratch_buffer.create(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, max_scratch_size);
	VkBufferDeviceAddressInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratch_buffer.handle};
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(vk::context().device, &buffer_info);

	// Allocate a query pool for storing the needed size for every BLAS
	// compaction.
	VkQueryPool queryPool{VK_NULL_HANDLE};
	if (nb_compactions > 0)	 // Is compaction requested?
	{
		assert(nb_compactions == nb_blas);	// Don't allow mix of on/off compaction
		VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		qpci.queryCount = nb_blas;
		qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		vkCreateQueryPool(vk::context().device, &qpci, nullptr, &queryPool);
	}
	// Batching creation/compaction of BLAS to allow staying in restricted
	// amount of memory
	std::vector<uint32_t> indices;	// Indices of the BLAS to create
	VkDeviceSize batchSize{0};
	VkDeviceSize batchLimit{256'000'000};  // 256 MB
	for (uint32_t idx = 0; idx < nb_blas; idx++) {
		indices.push_back(idx);
		batchSize += buildAs[idx].size_info.accelerationStructureSize;
		// Over the limit or last BLAS element
		if (batchSize >= batchLimit || idx == nb_blas - 1) {
			CommandBuffer cmdBuf(true, 0, vk::QueueType::GFX);
			cmd_create_blas(cmdBuf.handle, indices, buildAs, scratchAddress, queryPool);
			cmdBuf.submit();
			if (queryPool) {
				cmd_compact_blas(cmdBuf.handle, indices, buildAs, queryPool);
				cmdBuf.submit();
				// Destroy the non-compacted version
				for (auto i : indices) {
					vkDestroyAccelerationStructureKHR(vk::context().device, buildAs[i].cleanup_as.accel, nullptr);
					buildAs[i].cleanup_as.buffer.destroy();
				}
			}
			// Reset
			batchSize = 0;
			indices.clear();
		}
	}

	// Logging reduction
	if (queryPool) {
		VkDeviceSize compact_size =
			std::accumulate(buildAs.begin(), buildAs.end(), 0ULL,
							[](const auto& a, const auto& b) { return a + b.size_info.accelerationStructureSize; });
		LUMEN_TRACE(" RT BLAS: reducing from: %u to: %u = %u (%2.2f%s smaller) \n", as_total_size, compact_size,
					as_total_size - compact_size, (as_total_size - compact_size) / float(as_total_size) * 100.f, "%");
	}

	// Keeping all the created acceleration structures
	for (auto& b : buildAs) {
		blases.emplace_back(b.as);
	}
	// Clean up
	vkDestroyQueryPool(vk::context().device, queryPool, nullptr);
	scratch_buffer.destroy();
}

uint32_t prepare_frame() {
	vk::check(vkWaitForFences(vk::context().device, 1, &in_flight_fences[current_frame], VK_TRUE, 1000000000),
			  "Timeout");

	uint32_t image_idx;
	VkResult result = vkAcquireNextImageKHR(vk::context().device, vk::context().swapchain, UINT64_MAX,
											image_available_sem[current_frame], VK_NULL_HANDLE, &image_idx);
	if (result == VK_NOT_READY) {
		return UINT32_MAX;
	} else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		// Window resize
		vkDeviceWaitIdle(vk::context().device);
		recreate_swap_chain();
		cleanup_app_data();
		rg = std::make_unique<RenderGraph>();
		return UINT32_MAX;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		LUMEN_ERROR("Failed to acquire new swap chain image");
	}
	if (images_in_flight[image_idx] != VK_NULL_HANDLE) {
		vkWaitForFences(vk::context().device, 1, &images_in_flight[image_idx], VK_TRUE, UINT64_MAX);
	}

	vk::check(vkResetCommandBuffer(vk::context().command_buffers[image_idx], 0));
	vkResetFences(vk::context().device, 1, &in_flight_fences[current_frame]);
	images_in_flight[image_idx] = in_flight_fences[current_frame];
	return image_idx;
}

VkResult submit_frame(uint32_t image_idx) {
	VkSubmitInfo submit_info = vk::submit_info();
	VkSemaphore wait_semaphores[] = {image_available_sem[current_frame]};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk::context().command_buffers[image_idx];

	VkSemaphore signal_semaphores[] = {render_finished_sem[current_frame]};
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	vk::check(
		vkQueueSubmit(vk::context().queues[(int)vk::QueueType::GFX], 1, &submit_info, in_flight_fences[current_frame]),
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
		rg = std::make_unique<RenderGraph>();
		return result;
	} else if (result != VK_SUCCESS) {
		LUMEN_ERROR("Failed to present swap chain image");
	}

	return result;
}

RenderGraph* render_graph() { return rg.get(); }

// Build TLAS from an array of VkAccelerationStructureInstanceKHR
// - Use motion=true with VkAccelerationStructureMotionInstanceNV
// - The resulting TLAS will be stored in m_tlas
// - update is to rebuild the Tlas with updated matrices, flag must have the
// 'allow_update'
void build_tlas(AccelKHR& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances,
				VkBuildAccelerationStructureFlagsKHR flags, bool update) {
	// Cannot call buildTlas twice except to update.
	uint32_t count_instance = static_cast<uint32_t>(instances.size());

	// Command buffer to create the TLAS

	// Create a buffer holding the actual instance data (matrices++) for use by
	// the AS builder
	Buffer instances_buf;  // Buffer of instances containing the matrices and
						   // BLAS ids
	instances_buf.create(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						 sizeof(VkAccelerationStructureInstanceKHR) * instances.size(), instances.data(), true);
	VkBufferDeviceAddressInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instances_buf.handle};
	VkDeviceAddress instBufferAddr = vkGetBufferDeviceAddress(vk::context().device, &buffer_info);
	CommandBuffer cmd(true, 0, vk::QueueType::GFX);
	// Make sure the copy of the instance buffer are copied before triggering
	// the acceleration structure build
	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
						 nullptr);

	Buffer scratch_buffer;
	// Creating the TLAS
	cmd_create_tlas(tlas, cmd.handle, count_instance, scratch_buffer, instBufferAddr, flags, update);

	// Finalizing and destroying temporary data
	cmd.submit();
	instances_buf.destroy();
	scratch_buffer.destroy();
}

std::vector<Texture2D>& swapchain_images() {
	return _swapchain_images;
}
}  // namespace VulkanBase

}  // namespace lumen
