#pragma once
#include "LumenPCH.h"
#include "Buffer.h"
#include "Framework/Event.h"


struct VertexLayout {
	std::vector<vk::Component> components;
	VertexLayout(const std::vector<vk::Component>& components) : components(components) {}
	uint32_t stride();
};

struct VulkanBase {
	VulkanBase(bool validation_layers);
	// Create VKInstance with current extensions
	void create_instance();
	void setup_debug_messenger();
	void create_surface();
	void pick_physical_device();
	void create_logical_device();
	void create_swapchain();
	void create_sync_primitives();
	void create_command_buffers();
	void create_command_pool();
	void create_framebuffers(VkRenderPass render_pass);
	void cleanup_swapchain();
	void recreate_swap_chain(const std::function<void(VulkanContext&)>&, VulkanContext&);

	// Utils
	struct QueueFamilyIndices {
		std::optional<uint32_t> gfx_family;
		std::optional<uint32_t> present_family;
		std::optional<uint32_t> compute_family;

		// TODO: Extend to other families
		bool is_complete() {
			return gfx_family.has_value() &&
				present_family.has_value();
		}
	};

	struct SwapChainSupportDetails {

		VkSurfaceCapabilitiesKHR capabilities = {};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};
	bool check_validation_layer_support();
	std::vector<const char*> get_req_extensions();
	QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
	VkResult vkExt_create_debug_messenger(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);

	void vkExt_destroy_debug_messenger(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debug_messenger,
		const VkAllocationCallbacks* pAllocator);

	SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device);
	const int MAX_FRAMES_IN_FLIGHT = 2;

	const std::vector<const char*> validation_layers_lst = {
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	size_t current_frame = 0;
	//Sync primitives
	std::vector<VkSemaphore> image_available_sem;
	std::vector<VkSemaphore> render_finished_sem;
	std::vector<VkFence> in_flight_fences;
	std::vector<VkFence> images_in_flight;
	std::vector<VkQueueFamilyProperties> queue_families;
	VulkanContext ctx;
	bool enable_validation_layers;
	//int width;
	//int height;
	//bool fullscreen;
	//bool initialized = false;
	VkShaderModule create_shader(const std::vector<char>& code);
	void cleanup();
};

void create_default_render_pass(VulkanContext&);
