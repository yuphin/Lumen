#pragma once
#include "Framework/Logger.h"
#include <volk/volk.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>
#include <optional>
#include <vector>
#include "Utils.h"

namespace vk {

struct SamplerHash {
	size_t operator()(const VkSamplerCreateInfo& ci) const {
		size_t hash = 0;
		util::hash_combine(hash, ci.magFilter, ci.minFilter, ci.mipmapMode, ci.addressModeU, ci.addressModeV,
						   ci.addressModeW, ci.mipLodBias, ci.anisotropyEnable, ci.maxAnisotropy, ci.compareEnable,
						   ci.compareOp, ci.minLod, ci.maxLod, ci.borderColor, ci.unnormalizedCoordinates);

		return hash;
	}
};

struct QueueFamilyIndices {
	std::optional<uint32_t> gfx_family;
	std::optional<uint32_t> present_family;
	std::optional<uint32_t> compute_family;

	// TODO: Extend to other families
	bool is_complete() { return (gfx_family.has_value() && present_family.has_value()) && compute_family.has_value(); }
};

struct DescriptorInfo {
	union {
		VkDescriptorImageInfo image;
		VkDescriptorBufferInfo buffer;
	};
	DescriptorInfo() {}
	DescriptorInfo(const VkDescriptorImageInfo& image) { this->image = image; }

	DescriptorInfo(const VkDescriptorBufferInfo& buffer) { this->buffer = buffer; }
};

enum class QueueType { GFX, COMPUTE, PRESENT };

enum class LumenStage { L_STAGE_VERTEX, L_STAGE_FRAGMENT };
enum class Component { L_POSITION, L_NORMAL, L_COLOR, L_UV, L_TANGENT };
struct SpecializationMapEntry {
	std::vector<VkSpecializationMapEntry> entry;
	LumenStage shader_stage;
};

inline const char* vk_result_to_str(VkResult result) {
	switch (result) {
		case VK_SUCCESS:
			return "VK_SUCCESS";
		case VK_NOT_READY:
			return "VK_NOT_READY";
		case VK_TIMEOUT:
			return "VK_TIMEOUT";
		case VK_EVENT_SET:
			return "VK_EVENT_SET";
		case VK_EVENT_RESET:
			return "VK_EVENT_RESET";
		case VK_INCOMPLETE:
			return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST:
			return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED:
			return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS:
			return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL:
			return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_UNKNOWN:
			return "VK_ERROR_UNKNOWN";
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE:
			return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
		case VK_ERROR_FRAGMENTATION:
			return "VK_ERROR_FRAGMENTATION";
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
			return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		case VK_ERROR_SURFACE_LOST_KHR:
			return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR:
			return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR:
			return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
			return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT:
			return "VK_ERROR_VALIDATION_FAILED_EXT";
		default:
			return "VK_ERROR";
	}
}

inline void check(VkResult result, const char* msg = 0) {
	if (result != VK_SUCCESS) {
		LUMEN_ERROR(msg ? msg : vk_result_to_str(result));
	}
}

template <std::size_t Size>
inline void check(std::array<VkResult, Size> results, const char* msg = 0) {
	for (const auto& result : results) {
		if (result != VK_SUCCESS) {
			LUMEN_ERROR(msg ? msg : vk_result_to_str(result));
		}
	}
}

inline VkDebugUtilsMessengerCreateInfoEXT debug_messenger(PFN_vkDebugUtilsMessengerCallbackEXT debug_callback) {
	VkDebugUtilsMessengerCreateInfoEXT CI = {};
	CI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	CI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
						 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
						 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	CI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
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

inline VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool commandPool, VkCommandBufferLevel level,
																uint32_t bufferCount) {
	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = level;
	commandBufferAllocateInfo.commandBufferCount = bufferCount;
	return commandBufferAllocateInfo;
}

inline VkCommandPoolCreateInfo command_pool(VkCommandPoolCreateFlags flags = 0) {
	VkCommandPoolCreateInfo cmdPoolCreateInfo{};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.flags = flags;
	return cmdPoolCreateInfo;
}

inline VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0) {
	VkCommandBufferBeginInfo cmdBufferBeginInfo{};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = nullptr;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;
	cmdBufferBeginInfo.flags = flags;
	return cmdBufferBeginInfo;
}

inline VkImageMemoryBarrier image_memory_barrier() {
	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	return imageMemoryBarrier;
}

inline VkBufferMemoryBarrier buffer_memory_barrier() {
	VkBufferMemoryBarrier bufferMemoryBarrier{};
	bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	return bufferMemoryBarrier;
}

inline VkImageCreateInfo image(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent,
							   VkImageType image_type = VK_IMAGE_TYPE_2D, uint32_t mip_levels = 1,
							   uint32_t array_layers = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
							   VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
							   VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED) {
	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.imageType = image_type;
	info.extent = extent;
	info.mipLevels = mip_levels;
	info.arrayLayers = array_layers;
	info.format = format;
	info.tiling = tiling;
	info.usage = usageFlags;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = samples;
	info.initialLayout = initial_layout;
	return info;
}

inline VkImageViewCreateInfo image_view(VkImage img, VkFormat format,
										VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t level_count = 1,
										uint32_t layer_count = 1) {
	VkImageViewCreateInfo image_view_CI = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	image_view_CI.image = img;
	image_view_CI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_CI.format = format;
	image_view_CI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.subresourceRange.aspectMask = aspect;
	image_view_CI.subresourceRange.baseMipLevel = 0;
	image_view_CI.subresourceRange.levelCount = level_count;
	image_view_CI.subresourceRange.baseArrayLayer = 0;
	image_view_CI.subresourceRange.layerCount = layer_count;
	return image_view_CI;
}

inline VkSamplerCreateInfo sampler() {
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	return samplerCreateInfo;
}

inline VkImageViewCreateInfo image_view() {
	VkImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	return imageViewCreateInfo;
}

inline VkFramebufferCreateInfo framebuffer() {
	VkFramebufferCreateInfo framebufferCreateInfo{};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	return framebufferCreateInfo;
}

inline VkSemaphoreCreateInfo semaphore() {
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	return semaphoreCreateInfo;
}

inline VkFenceCreateInfo fence(VkFenceCreateFlags flags = 0) {
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = flags;
	return fenceCreateInfo;
}

inline VkEventCreateInfo event() {
	VkEventCreateInfo eventCreateInfo{};
	eventCreateInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	return eventCreateInfo;
}

inline VkSubmitInfo submit_info() {
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	return submitInfo;
}

inline VkViewport viewport(float width, float height, float minDepth, float maxDepth) {
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	return viewport;
}

inline VkViewport viewport2(float width, float height, float minDepth, float maxDepth) {
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = height;
	viewport.width = width;
	viewport.height = -height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	return viewport;
}

inline VkRect2D rect2D(int32_t width, int32_t height, int32_t offsetX, int32_t offsetY) {
	VkRect2D rect2D{};
	rect2D.extent.width = width;
	rect2D.extent.height = height;
	rect2D.offset.x = offsetX;
	rect2D.offset.y = offsetY;
	return rect2D;
}

inline VkBufferCreateInfo buffer() {
	VkBufferCreateInfo bufCreateInfo{};
	bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	return bufCreateInfo;
}

inline VkBufferCreateInfo buffer(VkBufferUsageFlags usage, VkDeviceSize size, VkSharingMode sharing_mode) {
	VkBufferCreateInfo buffer_CI{};
	buffer_CI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_CI.usage = usage;
	buffer_CI.size = size;
	buffer_CI.sharingMode = sharing_mode;
	return buffer_CI;
}

inline VkBufferCreateInfo buffer(VkBufferUsageFlags usage, VkDeviceSize size) {
	VkBufferCreateInfo buffer_CI{};
	buffer_CI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_CI.usage = usage;
	buffer_CI.size = size;
	return buffer_CI;
}

inline VkDescriptorPoolCreateInfo descriptor_pool(size_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes,
												  size_t maxSets) {
	VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizeCount);
	descriptorPoolInfo.pPoolSizes = pPoolSizes;
	descriptorPoolInfo.maxSets = static_cast<uint32_t>(maxSets);
	return descriptorPoolInfo;
}

inline VkDescriptorPoolCreateInfo descriptor_pool(const std::vector<VkDescriptorPoolSize>& poolSizes, size_t maxSets) {
	VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	descriptorPoolInfo.maxSets = static_cast<uint32_t>(maxSets);
	return descriptorPoolInfo;
}

inline VkDescriptorPoolSize descriptor_pool_size(VkDescriptorType type, size_t descriptorCount) {
	VkDescriptorPoolSize descriptorPoolSize{};
	descriptorPoolSize.type = type;
	descriptorPoolSize.descriptorCount = static_cast<uint32_t>(descriptorCount);
	return descriptorPoolSize;
}

inline VkDescriptorSetLayoutBinding descriptor_set_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags,
																  uint32_t binding, size_t descriptorCount = 1) {
	VkDescriptorSetLayoutBinding setLayoutBinding{};
	setLayoutBinding.descriptorType = type;
	setLayoutBinding.stageFlags = stageFlags;
	setLayoutBinding.binding = binding;
	setLayoutBinding.descriptorCount = static_cast<uint32_t>(descriptorCount);
	return setLayoutBinding;
}

inline VkDescriptorSetLayoutCreateInfo descriptor_set_layout(const VkDescriptorSetLayoutBinding* pBindings,
															 size_t bindingCount) {
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pBindings = pBindings;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindingCount);
	return descriptorSetLayoutCreateInfo;
}

inline VkDescriptorSetLayoutCreateInfo descriptor_set_layout(
	const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pBindings = bindings.data();
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	return descriptorSetLayoutCreateInfo;
}

inline VkPipelineLayoutCreateInfo pipeline_layout(const VkDescriptorSetLayout* pSetLayouts,
												  uint32_t setLayoutCount = 1) {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
	pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
	return pipelineLayoutCreateInfo;
}

inline VkPipelineLayoutCreateInfo pipeline_layout(uint32_t setLayoutCount = 1) {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
	return pipelineLayoutCreateInfo;
}

inline VkDescriptorSetAllocateInfo descriptor_set_allocate_info(VkDescriptorPool descriptorPool,
																const VkDescriptorSetLayout* pSetLayouts,
																size_t descriptorSetCount) {
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
	descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetCount);
	return descriptorSetAllocateInfo;
}

inline VkDescriptorImageInfo descriptor_image_info(VkSampler sampler, VkImageView imageView,
												   VkImageLayout imageLayout) {
	VkDescriptorImageInfo descriptorImageInfo{};
	descriptorImageInfo.sampler = sampler;
	descriptorImageInfo.imageView = imageView;
	descriptorImageInfo.imageLayout = imageLayout;
	return descriptorImageInfo;
}

inline VkWriteDescriptorSet write_descriptor_set(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding,
												 VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount = 1) {
	VkWriteDescriptorSet writeDescriptorSet{};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = dstSet;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.pBufferInfo = bufferInfo;
	writeDescriptorSet.descriptorCount = descriptorCount;
	return writeDescriptorSet;
}

inline VkWriteDescriptorSet write_descriptor_set(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding,
												 VkDescriptorImageInfo* imageInfo, uint32_t descriptorCount = 1) {
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
inline VkWriteDescriptorSet write_descriptor_set(VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding,
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
inline VkVertexInputBindingDescription vertex_input_binding_description(uint32_t binding, uint32_t stride,
																		VkVertexInputRate inputRate) {
	VkVertexInputBindingDescription vInputBindDescription{};
	vInputBindDescription.binding = binding;
	vInputBindDescription.stride = stride;
	vInputBindDescription.inputRate = inputRate;
	return vInputBindDescription;
}

inline VkVertexInputAttributeDescription vertex_input_attribute_description(uint32_t binding, uint32_t location,
																			VkFormat format, uint32_t offset) {
	VkVertexInputAttributeDescription vInputAttribDescription{};
	vInputAttribDescription.location = location;
	vInputAttribDescription.binding = binding;
	vInputAttribDescription.format = format;
	vInputAttribDescription.offset = offset;
	return vInputAttribDescription;
}

inline VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state() {
	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
	pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	return pipelineVertexInputStateCreateInfo;
}

inline VkPipelineInputAssemblyStateCreateInfo pipeline_vertex_input_assembly_state(
	VkPrimitiveTopology topology, VkPipelineInputAssemblyStateCreateFlags flags, VkBool32 primitiveRestartEnable) {
	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
	pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineInputAssemblyStateCreateInfo.topology = topology;
	pipelineInputAssemblyStateCreateInfo.flags = flags;
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
	return pipelineInputAssemblyStateCreateInfo;
}

inline VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state(
	VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace,
	VkPipelineRasterizationStateCreateFlags flags = 0) {
	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
	pipelineRasterizationStateCreateInfo.cullMode = cullMode;
	pipelineRasterizationStateCreateInfo.frontFace = frontFace;
	pipelineRasterizationStateCreateInfo.flags = flags;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
	return pipelineRasterizationStateCreateInfo;
}

inline VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state(VkColorComponentFlags colorWriteMask,
																				 VkBool32 blendEnable) {
	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
	pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
	pipelineColorBlendAttachmentState.blendEnable = blendEnable;
	return pipelineColorBlendAttachmentState;
}

inline VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state(
	uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState* pAttachments) {
	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
	pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
	return pipelineColorBlendStateCreateInfo;
}

inline VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable,
																	VkCompareOp depthCompareOp) {
	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
	pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
	return pipelineDepthStencilStateCreateInfo;
}

inline VkPipelineViewportStateCreateInfo pipeline_viewport_state(uint32_t viewportCount, uint32_t scissorCount,
																 VkPipelineViewportStateCreateFlags flags = 0) {
	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = viewportCount;
	pipelineViewportStateCreateInfo.scissorCount = scissorCount;
	pipelineViewportStateCreateInfo.flags = flags;
	return pipelineViewportStateCreateInfo;
}

inline VkPipelineMultisampleStateCreateInfo pipeline_multisample_state(
	VkSampleCountFlagBits rasterizationSamples, VkPipelineMultisampleStateCreateFlags flags = 0) {
	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
	pipelineMultisampleStateCreateInfo.flags = flags;
	return pipelineMultisampleStateCreateInfo;
}

inline VkPipelineDynamicStateCreateInfo pipeline_dynamic_state(const VkDynamicState* pDynamicStates,
															   uint32_t dynamicStateCount,
															   VkPipelineDynamicStateCreateFlags flags = 0) {
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
	pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
	pipelineDynamicStateCreateInfo.flags = flags;
	return pipelineDynamicStateCreateInfo;
}

inline VkPipelineDynamicStateCreateInfo pipeline_dynamic_state(const std::vector<VkDynamicState>& pDynamicStates,
															   VkPipelineDynamicStateCreateFlags flags = 0) {
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
	pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pDynamicStates.size());
	pipelineDynamicStateCreateInfo.flags = flags;
	return pipelineDynamicStateCreateInfo;
}

inline VkPipelineTessellationStateCreateInfo pipeline_tesellation_state(uint32_t patchControlPoints) {
	VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo{};
	pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	pipelineTessellationStateCreateInfo.patchControlPoints = patchControlPoints;
	return pipelineTessellationStateCreateInfo;
}

inline VkGraphicsPipelineCreateInfo graphics_pipeline(VkPipelineLayout layout, VkRenderPass renderPass,
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

inline VkGraphicsPipelineCreateInfo graphics_pipeline() {
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	return pipelineCreateInfo;
}

inline VkComputePipelineCreateInfo compute_pipeline(VkPipelineLayout layout, VkPipelineCreateFlags flags = 0) {
	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.layout = layout;
	computePipelineCreateInfo.flags = flags;
	return computePipelineCreateInfo;
}

inline VkPushConstantRange push_constant_range(VkShaderStageFlags stageFlags, uint32_t size, uint32_t offset) {
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
		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			return VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		default:
			return VkAccessFlags();
	}
}

inline VkDependencyInfo dependency_info(int cnt, const VkBufferMemoryBarrier2* p_buffer_memory_barriers) {
	VkDependencyInfo res = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	res.dependencyFlags = 0;
	res.bufferMemoryBarrierCount = cnt;
	res.pBufferMemoryBarriers = p_buffer_memory_barriers;
	return res;
}

inline VkDependencyInfo dependency_info(uint32_t cnt, const VkImageMemoryBarrier2* p_img_memory_barriers) {
	VkDependencyInfo res = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	res.dependencyFlags = 0;
	res.imageMemoryBarrierCount = cnt;
	res.pImageMemoryBarriers = p_img_memory_barriers;
	return res;
}

inline VkRenderingAttachmentInfo rendering_attachment_info(VkImageView image_view, VkImageLayout image_layout,
														   VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op,
														   VkClearValue clear_value) {
	VkRenderingAttachmentInfo res = {};
	res.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	res.imageView = image_view;
	res.imageLayout = image_layout;
	res.loadOp = load_op;
	res.storeOp = store_op;
	res.clearValue = clear_value;

	return res;
}

inline VkRenderingAttachmentInfo rendering_attachment_info(VkImageLayout image_layout, VkAttachmentLoadOp load_op,
														   VkAttachmentStoreOp store_op, VkClearValue clear_value) {
	VkRenderingAttachmentInfo res = {};
	res.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	res.imageLayout = image_layout;
	res.loadOp = load_op;
	res.storeOp = store_op;
	res.clearValue = clear_value;

	return res;
}

}  // namespace vk