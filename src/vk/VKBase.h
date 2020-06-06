#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VKStructs.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <set>
#include <optional>

class VKBase {

	void init_vulkan();
	void init_window();
	void draw_frame();
	// Create VKInstance with the current extensions
	void create_instance();
	void setup_debug_messenger();
	void create_surface();
	void pick_physical_device();
	void create_logical_device();
	void create_swapchain();
	void create_image_views();
	void create_sync_primitives();
	void cleanup_swapchain();
	void recreate_swap_chain();


	// Utils
	// Taken from https://vulkan-tutorial.com
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
		VkSurfaceCapabilitiesKHR capabilities;
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

	protected:

	bool enable_validation_layers;
	int width;
	int height;
	bool fullscreen;

	GLFWwindow* window = nullptr;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkSurfaceKHR surface;

	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;
	VkQueue gfx_queue;
	VkQueue present_queue;
	VkQueue compute_queue;

	VkRenderPass render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline gfx_pipeline;

	// Swapchain related stuff
	VkFormat swapchain_image_format;
	VkExtent2D swapchain_extent;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	std::vector<VkFramebuffer> swapchain_framebuffers;

	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;

	std::vector<char> read_file(const std::string& filename);
	VkShaderModule create_shader(const std::vector<char>& code);
	void init();
	void cleanup();
	void render_loop();
	virtual void create_render_pass();
	virtual void create_gfx_pipeline();
	virtual void create_framebuffers();
	virtual void create_command_pool();
	virtual void create_command_buffers();

	public:
	bool resized = false;
	VKBase(int width, int height, bool fullscreen, bool validation_layers);


};


