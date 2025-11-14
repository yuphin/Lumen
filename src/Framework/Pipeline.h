#pragma once
#include "Shader.h"
#include "Texture.h"
#include "SBTWrapper.h"
#include "RenderGraphTypes.h"

namespace vk {
inline constexpr u32 MAX_AS_BINDING_COUNT = 2;
struct Pipeline;

struct Pipeline {
   public:
	enum class PipelineType { GFX = 0, RT = 1, COMPUTE = 2 };
	Pipeline(const std::string& name);
	void cleanup();
	void create_gfx_pipeline(lm::Arena* arena, const GraphicsPassSettings& settings,
							 const lm::FixedArray<u32>& descriptor_counts, std::vector<vk::Texture*> color_outputs,
							 vk::Texture* depth_output);
	void create_rt_pipeline(lm::Arena* arena, const RTPassSettings& settings,
							const lm::FixedArray<u32>& descriptor_counts, u32 num_as_bindings);
	void create_compute_pipeline(lm::Arena* arena, const ComputePassSettings& settings,
								 const lm::FixedArray<u32>& descriptor_counts);
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_rt_regions();

	VkPipeline handle = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
	PipelineType type;
	VkDescriptorUpdateTemplate update_template = nullptr;
	VkShaderStageFlags pc_stages = 0;
	std::string name;
	u32 push_constant_size = 0;
	VkDescriptorType descriptor_types[32] = {};

	/*
		Potentially 1 descriptor pool for a pass where we have to keep the
		TLAS descriptor, because we can't push its descriptor with a template as
		of Vulkan 1.3
	*/
	// RT specific data
	VkDescriptorPool tlas_descriptor_pool = nullptr;
	VkDescriptorSet tlas_descriptor_set = nullptr;
	VkDescriptorSetLayout tlas_layout = VK_NULL_HANDLE;
	VkWriteDescriptorSetAccelerationStructureKHR tlas_info = {};
	SBTWrapper sbt_wrapper;

   private:
	void create_pipeline_layout(const std::vector<Shader>& shaders, const std::vector<u32> push_const_sizes);
	void create_update_template(const std::vector<Shader>& shaders, const lm::FixedArray<u32>& descriptor_counts);
	void create_set_layout(const std::vector<Shader>& shaders, const lm::FixedArray<u32>& descriptor_counts);
	void create_rt_set_layout(VkShaderStageFlags stage_flags, u32 num_as_bindings);
	u32 binding_mask;
};

}  // namespace vk
