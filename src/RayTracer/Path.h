#pragma once
#include "Integrator.h"
class Path : public Integrator {
public:
	Path(LumenInstance* scene) : scene(scene) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
private:
	void create_offscreen_resources();
	void create_descriptors();
	void create_blas();
	void create_tlas();
	void create_rt_pipelines();
	void update_uniform_buffers();
	LumenInstance* scene;
	LumenScene lumen_scene;
	PushConstantRay pc_ray{};
	std::vector<MeshLight> lights;
	VkSampler texture_sampler;
	std::vector<Texture2D> textures;
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	std::unique_ptr<Pipeline> rt_pipeline;
};

