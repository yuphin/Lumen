#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct AccelKHR;


// Utils
struct QueueFamilyIndices {
	std::optional<uint32_t> gfx_family;
	std::optional<uint32_t> present_family;
	std::optional<uint32_t> compute_family;

	// TODO: Extend to other families
	bool is_complete() {
		return (gfx_family.has_value() && present_family.has_value()) &&
			compute_family.has_value();
	}
};

struct VulkanContext {
	GLFWwindow* window_ptr = nullptr;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkSurfaceKHR surface;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;
	// VkQueue gfx_queue;
	// VkQueue present_queue;
	// VkQueue compute_queue;
	VkRenderPass default_render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline gfx_pipeline;
	// Swapchain related stuff
	VkFormat swapchain_image_format;
	VkExtent2D swapchain_extent;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	std::vector<VkFramebuffer> swapchain_framebuffers;
	std::vector<VkCommandPool> cmd_pools;
	std::vector<VkQueue> queues;
	QueueFamilyIndices indices;
	std::vector<VkCommandBuffer> command_buffers;
	VkPhysicalDeviceFeatures supported_features;
	VkPhysicalDeviceProperties device_properties;
	VkPhysicalDeviceProperties2 device_properties2;
	VkPhysicalDeviceMemoryProperties memory_properties;

	VkImage depth_img;
	VkDeviceMemory depth_img_memory;
	VkImageView depth_img_view;
};

enum class QueueType { GFX, COMPUTE, PRESENT };

struct BlasInput {
	// Data used to build acceleration structure geometry
	std::vector<VkAccelerationStructureGeometryKHR> as_geom;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> as_build_offset_info;
	VkBuildAccelerationStructureFlagsKHR flags{ 0 };
};

namespace vk {

	enum class LumenStage { L_STAGE_VERTEX, L_STAGE_FRAGMENT };
	enum class Component { L_POSITION, L_NORMAL, L_COLOR, L_UV, L_TANGENT };
	struct SpecializationMapEntry {
		std::vector<VkSpecializationMapEntry> entry;
		LumenStage shader_stage;
	};
	inline void check(VkResult result, const char* msg = 0) {
		if (result != VK_SUCCESS && msg) {
		LUMEN_ERROR(msg);
		}
	}

	template <std::size_t Size>
	inline void check(std::array<VkResult, Size> results, const char* msg = 0) {
		for (const auto& result : results) {
			if (result != VK_SUCCESS && msg) {
				LUMEN_ERROR(msg);
			}
		}
	}

	inline VkDebugUtilsMessengerCreateInfoEXT
		debug_messenger_CI(PFN_vkDebugUtilsMessengerCallbackEXT debug_callback) {
		VkDebugUtilsMessengerCreateInfoEXT CI = {};
		CI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		CI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		CI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		CI.pfnUserCallback = debug_callback;
		return CI;
	}

	inline VkMemoryAllocateInfo memory_allocate_info() {
		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		return memAllocInfo;
	}

	inline VkMappedMemoryRange mapped_memory_range() {
		VkMappedMemoryRange mappedMemoryRange{};
		mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		return mappedMemoryRange;
	}

	inline VkCommandBufferAllocateInfo
		command_buffer_allocate_info(VkCommandPool commandPool,
									 VkCommandBufferLevel level, uint32_t bufferCount) {
		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType =
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = level;
		commandBufferAllocateInfo.commandBufferCount = bufferCount;
		return commandBufferAllocateInfo;
	}

	inline VkCommandPoolCreateInfo
		command_pool_CI(VkCommandPoolCreateFlags flags = 0) {
		VkCommandPoolCreateInfo cmdPoolCreateInfo{};
		cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolCreateInfo.flags = flags;
		return cmdPoolCreateInfo;
	}

	inline VkCommandBufferBeginInfo
		command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0) {
		VkCommandBufferBeginInfo cmdBufferBeginInfo{};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufferBeginInfo.pNext = nullptr;
		cmdBufferBeginInfo.pInheritanceInfo = nullptr;
		cmdBufferBeginInfo.flags = flags;
		return cmdBufferBeginInfo;
	}

	inline VkCommandBufferInheritanceInfo command_buffer_inheritance_info() {
		VkCommandBufferInheritanceInfo cmdBufferInheritanceInfo{};
		cmdBufferInheritanceInfo.sType =
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		return cmdBufferInheritanceInfo;
	}

	inline VkRenderPassBeginInfo render_pass_begin_info() {
		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		return renderPassBeginInfo;
	}

	inline VkRenderPassCreateInfo render_pass_CI() {
		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		return renderPassCreateInfo;
	}

	/** @brief Initialize an image memory barrier with no image transfer ownership
	 */
	inline VkImageMemoryBarrier image_memory_barrier() {
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		return imageMemoryBarrier;
	}

	/** @brief Initialize a buffer memory barrier with no image transfer ownership
	 */
	inline VkBufferMemoryBarrier buffer_memory_barrier() {
		VkBufferMemoryBarrier bufferMemoryBarrier{};
		bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		return bufferMemoryBarrier;
	}

	inline VkMemoryBarrier memory_barrier() {
		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		return memoryBarrier;
	}

	inline VkImageCreateInfo image_create_info(VkFormat format,
											   VkImageUsageFlags usageFlags,
											   VkExtent3D extent) {
		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;

		info.imageType = VK_IMAGE_TYPE_2D;

		info.format = format;
		info.extent = extent;

		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = usageFlags;

		return info;
	}

	inline VkSamplerCreateInfo sampler_create_info() {
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		return samplerCreateInfo;
	}

	inline VkImageViewCreateInfo image_view_CI() {
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		return imageViewCreateInfo;
	}

	inline VkFramebufferCreateInfo framebuffer_create_info() {
		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		return framebufferCreateInfo;
	}

	inline VkSemaphoreCreateInfo semaphore_create_info() {
		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		return semaphoreCreateInfo;
	}

	inline VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0) {
		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = flags;
		return fenceCreateInfo;
	}

	inline VkEventCreateInfo event_create_info() {
		VkEventCreateInfo eventCreateInfo{};
		eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
		return eventCreateInfo;
	}

	inline VkSubmitInfo submit_info() {
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		return submitInfo;
	}

	inline VkViewport viewport(float width, float height, float minDepth,
							   float maxDepth) {
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		return viewport;
	}

	inline VkViewport viewport2(float width, float height, float minDepth,
								float maxDepth) {
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = height;
		viewport.width = width;
		viewport.height = -height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		return viewport;
	}

	inline VkRect2D rect2D(int32_t width, int32_t height, int32_t offsetX,
						   int32_t offsetY) {
		VkRect2D rect2D{};
		rect2D.extent.width = width;
		rect2D.extent.height = height;
		rect2D.offset.x = offsetX;
		rect2D.offset.y = offsetY;
		return rect2D;
	}

	inline VkBufferCreateInfo buffer_create_info() {
		VkBufferCreateInfo bufCreateInfo{};
		bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		return bufCreateInfo;
	}

	inline VkBufferCreateInfo buffer_create_info(VkBufferUsageFlags usage,
												 VkDeviceSize size,
												 VkSharingMode sharing_mode) {
		VkBufferCreateInfo buffer_CI{};
		buffer_CI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_CI.usage = usage;
		buffer_CI.size = size;
		buffer_CI.sharingMode = sharing_mode;
		return buffer_CI;
	}

	inline VkBufferCreateInfo buffer_create_info(VkBufferUsageFlags usage,
												 VkDeviceSize size) {
		VkBufferCreateInfo buffer_CI{};
		buffer_CI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_CI.usage = usage;
		buffer_CI.size = size;
		return buffer_CI;
	}

	inline VkDescriptorPoolCreateInfo
		descriptor_pool_CI(size_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes,
						   size_t maxSets) {
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizeCount);
		descriptorPoolInfo.pPoolSizes = pPoolSizes;
		descriptorPoolInfo.maxSets = static_cast<uint32_t>(maxSets);
		return descriptorPoolInfo;
	}

	inline VkDescriptorPoolCreateInfo
		descriptor_pool_CI(const std::vector<VkDescriptorPoolSize>& poolSizes,
						   size_t maxSets) {
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = static_cast<uint32_t>(maxSets);
		return descriptorPoolInfo;
	}

	inline VkDescriptorPoolSize descriptor_pool_size(VkDescriptorType type,
													 size_t descriptorCount) {
		VkDescriptorPoolSize descriptorPoolSize{};
		descriptorPoolSize.type = type;
		descriptorPoolSize.descriptorCount = static_cast<uint32_t>(descriptorCount);
		return descriptorPoolSize;
	}

	inline VkDescriptorSetLayoutBinding
		descriptor_set_layout_binding(VkDescriptorType type,
									  VkShaderStageFlags stageFlags, uint32_t binding,
									  size_t descriptorCount = 1) {
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		setLayoutBinding.descriptorType = type;
		setLayoutBinding.stageFlags = stageFlags;
		setLayoutBinding.binding = binding;
		setLayoutBinding.descriptorCount = static_cast<uint32_t>(descriptorCount);
		return setLayoutBinding;
	}

	inline VkDescriptorSetLayoutCreateInfo
		descriptor_set_layout_CI(const VkDescriptorSetLayoutBinding* pBindings,
								 size_t bindingCount) {
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType =
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pBindings = pBindings;
		descriptorSetLayoutCreateInfo.bindingCount =
			static_cast<uint32_t>(bindingCount);
		return descriptorSetLayoutCreateInfo;
	}

	inline VkDescriptorSetLayoutCreateInfo descriptor_set_layout_CI(
		const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType =
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pBindings = bindings.data();
		descriptorSetLayoutCreateInfo.bindingCount =
			static_cast<uint32_t>(bindings.size());
		return descriptorSetLayoutCreateInfo;
	}

	inline VkPipelineLayoutCreateInfo
		pipeline_layout_CI(const VkDescriptorSetLayout* pSetLayouts,
						   uint32_t setLayoutCount = 1) {
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
		pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
		return pipelineLayoutCreateInfo;
	}

	inline VkPipelineLayoutCreateInfo
		pipeline_layout_CI(uint32_t setLayoutCount = 1) {
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
		return pipelineLayoutCreateInfo;
	}

	inline VkDescriptorSetAllocateInfo
		descriptor_set_allocate_info(VkDescriptorPool descriptorPool,
									 const VkDescriptorSetLayout* pSetLayouts,
									 size_t descriptorSetCount) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType =
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorPool;
		descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
		descriptorSetAllocateInfo.descriptorSetCount =
			static_cast<uint32_t>(descriptorSetCount);
		return descriptorSetAllocateInfo;
	}

	inline VkDescriptorImageInfo descriptor_image_info(VkSampler sampler,
													   VkImageView imageView,
													   VkImageLayout imageLayout) {
		VkDescriptorImageInfo descriptorImageInfo{};
		descriptorImageInfo.sampler = sampler;
		descriptorImageInfo.imageView = imageView;
		descriptorImageInfo.imageLayout = imageLayout;
		return descriptorImageInfo;
	}

	inline VkWriteDescriptorSet
		write_descriptor_set(VkDescriptorSet dstSet, VkDescriptorType type,
							 uint32_t binding, VkDescriptorBufferInfo* bufferInfo,
							 uint32_t descriptorCount = 1) {
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = dstSet;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pBufferInfo = bufferInfo;
		writeDescriptorSet.descriptorCount = descriptorCount;
		return writeDescriptorSet;
	}

	inline VkWriteDescriptorSet
		write_descriptor_set(VkDescriptorSet dstSet, VkDescriptorType type,
							 uint32_t binding, VkDescriptorImageInfo* imageInfo,
							 uint32_t descriptorCount = 1) {
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = dstSet;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pImageInfo = imageInfo;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorCount = descriptorCount;
		return writeDescriptorSet;
	}
#if VK_KHR_acceleration_structure
	inline VkWriteDescriptorSet
		write_descriptor_set(VkDescriptorSet dstSet, VkDescriptorType type,
							 uint32_t binding,
							 const VkWriteDescriptorSetAccelerationStructureKHR* pAccel,
							 uint32_t descriptorCount = 1, uint32_t arrayElement = 0) {
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = dstSet;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.descriptorCount = descriptorCount;
		writeDescriptorSet.dstArrayElement = arrayElement;
		writeDescriptorSet.pNext = pAccel;
		return writeDescriptorSet;
	}
#endif
	inline VkVertexInputBindingDescription
		vertex_input_binding_description(uint32_t binding, uint32_t stride,
										 VkVertexInputRate inputRate) {
		VkVertexInputBindingDescription vInputBindDescription{};
		vInputBindDescription.binding = binding;
		vInputBindDescription.stride = stride;
		vInputBindDescription.inputRate = inputRate;
		return vInputBindDescription;
	}

	inline VkVertexInputAttributeDescription
		vertex_input_attribute_description(uint32_t binding, uint32_t location,
										   VkFormat format, uint32_t offset) {
		VkVertexInputAttributeDescription vInputAttribDescription{};
		vInputAttribDescription.location = location;
		vInputAttribDescription.binding = binding;
		vInputAttribDescription.format = format;
		vInputAttribDescription.offset = offset;
		return vInputAttribDescription;
	}

	inline VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_CI() {
		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
		pipelineVertexInputStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		return pipelineVertexInputStateCreateInfo;
	}

	inline VkPipelineInputAssemblyStateCreateInfo
		pipeline_vertex_input_assembly_state_CI(
			VkPrimitiveTopology topology, VkPipelineInputAssemblyStateCreateFlags flags,
			VkBool32 primitiveRestartEnable) {
		VkPipelineInputAssemblyStateCreateInfo
			pipelineInputAssemblyStateCreateInfo{};
		pipelineInputAssemblyStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.topology = topology;
		pipelineInputAssemblyStateCreateInfo.flags = flags;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable =
			primitiveRestartEnable;
		return pipelineInputAssemblyStateCreateInfo;
	}

	inline VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_CI(
		VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace,
		VkPipelineRasterizationStateCreateFlags flags = 0) {
		VkPipelineRasterizationStateCreateInfo
			pipelineRasterizationStateCreateInfo{};
		pipelineRasterizationStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
		pipelineRasterizationStateCreateInfo.cullMode = cullMode;
		pipelineRasterizationStateCreateInfo.frontFace = frontFace;
		pipelineRasterizationStateCreateInfo.flags = flags;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
		return pipelineRasterizationStateCreateInfo;
	}

	inline VkPipelineColorBlendAttachmentState
		pipeline_color_blend_attachment_state(VkColorComponentFlags colorWriteMask,
											  VkBool32 blendEnable) {
		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
		pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
		pipelineColorBlendAttachmentState.blendEnable = blendEnable;
		return pipelineColorBlendAttachmentState;
	}

	inline VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_CI(
		uint32_t attachmentCount,
		const VkPipelineColorBlendAttachmentState* pAttachments) {
		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
		pipelineColorBlendStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
		pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
		return pipelineColorBlendStateCreateInfo;
	}

	inline VkPipelineDepthStencilStateCreateInfo
		pipeline_depth_stencil_CI(VkBool32 depthTestEnable, VkBool32 depthWriteEnable,
								  VkCompareOp depthCompareOp) {
		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
		pipelineDepthStencilStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
		pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
		pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
		pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
		return pipelineDepthStencilStateCreateInfo;
	}

	inline VkPipelineViewportStateCreateInfo
		pipeline_viewport_state_CI(uint32_t viewportCount, uint32_t scissorCount,
								   VkPipelineViewportStateCreateFlags flags = 0) {
		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
		pipelineViewportStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		pipelineViewportStateCreateInfo.viewportCount = viewportCount;
		pipelineViewportStateCreateInfo.scissorCount = scissorCount;
		pipelineViewportStateCreateInfo.flags = flags;
		return pipelineViewportStateCreateInfo;
	}

	inline VkPipelineMultisampleStateCreateInfo
		pipeline_multisample_state_CI(VkSampleCountFlagBits rasterizationSamples,
									  VkPipelineMultisampleStateCreateFlags flags = 0) {
		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
		pipelineMultisampleStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		pipelineMultisampleStateCreateInfo.rasterizationSamples =
			rasterizationSamples;
		pipelineMultisampleStateCreateInfo.flags = flags;
		return pipelineMultisampleStateCreateInfo;
	}

	inline VkPipelineDynamicStateCreateInfo
		pipeline_dynamic_state_CI(const VkDynamicState* pDynamicStates,
								  uint32_t dynamicStateCount,
								  VkPipelineDynamicStateCreateFlags flags = 0) {
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
		pipelineDynamicStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
		pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
		pipelineDynamicStateCreateInfo.flags = flags;
		return pipelineDynamicStateCreateInfo;
	}

	inline VkPipelineDynamicStateCreateInfo
		pipeline_dynamic_state_CI(const std::vector<VkDynamicState>& pDynamicStates,
								  VkPipelineDynamicStateCreateFlags flags = 0) {
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
		pipelineDynamicStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
		pipelineDynamicStateCreateInfo.dynamicStateCount =
			static_cast<uint32_t>(pDynamicStates.size());
		pipelineDynamicStateCreateInfo.flags = flags;
		return pipelineDynamicStateCreateInfo;
	}

	inline VkPipelineTessellationStateCreateInfo
		pipeline_tesellation_state_CI(uint32_t patchControlPoints) {
		VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo{};
		pipelineTessellationStateCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		pipelineTessellationStateCreateInfo.patchControlPoints = patchControlPoints;
		return pipelineTessellationStateCreateInfo;
	}

	inline VkGraphicsPipelineCreateInfo
		graphics_pipeline_CI(VkPipelineLayout layout, VkRenderPass renderPass,
							 VkPipelineCreateFlags flags = 0) {
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = layout;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.flags = flags;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		return pipelineCreateInfo;
	}

	inline VkGraphicsPipelineCreateInfo graphics_pipeline_CI() {
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		return pipelineCreateInfo;
	}

	inline VkComputePipelineCreateInfo
		compute_pipeline_CI(VkPipelineLayout layout, VkPipelineCreateFlags flags = 0) {
		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType =
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = layout;
		computePipelineCreateInfo.flags = flags;
		return computePipelineCreateInfo;
	}

	inline VkPushConstantRange push_constant_range(VkShaderStageFlags stageFlags,
												   uint32_t size, uint32_t offset) {
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = stageFlags;
		pushConstantRange.offset = offset;
		pushConstantRange.size = size;
		return pushConstantRange;
	}

	inline VkBindSparseInfo bind_sparse_info() {
		VkBindSparseInfo bindSparseInfo{};
		bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
		return bindSparseInfo;
	}

	/** @brief Initialize a map entry for a shader specialization constant */
	inline VkSpecializationMapEntry
		specializaiton_map_entry(uint32_t constantID, uint32_t offset, size_t size) {
		VkSpecializationMapEntry specializationMapEntry{};
		specializationMapEntry.constantID = constantID;
		specializationMapEntry.offset = offset;
		specializationMapEntry.size = size;
		return specializationMapEntry;
	}

	/** @brief Initialize a specialization constant info structure to pass to a
	 * shader stage */
	inline VkSpecializationInfo
		specialization_info(const std::vector<VkSpecializationMapEntry>& mapEntries,
							size_t dataSize, const void* data) {
		VkSpecializationInfo specializationInfo{};
		specializationInfo.mapEntryCount = static_cast<uint32_t>(mapEntries.size());
		specializationInfo.pMapEntries = mapEntries.data();
		specializationInfo.dataSize = dataSize;
		specializationInfo.pData = data;
		return specializationInfo;
	}

	inline VkAccessFlags access_flags_for_img_layout(VkImageLayout layout) {
		switch (layout) {
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return VK_ACCESS_HOST_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_ACCESS_TRANSFER_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_ACCESS_TRANSFER_READ_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_ACCESS_SHADER_READ_BIT;
			default:
				return VkAccessFlags();
		}
	}

	inline VkPipelineStageFlags
		pipeline_stage_for_img_layout(VkImageLayout layout) {
		switch (layout) {
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // We do this to allow queue
														   // other than graphic return
														   // VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // We do this to allow queue
														   // other than graphic return
														   // VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return VK_PIPELINE_STAGE_HOST_BIT;
			case VK_IMAGE_LAYOUT_UNDEFINED:
				return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			default:
				return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}
	}

} // namespace vk
