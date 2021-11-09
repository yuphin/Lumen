#pragma once

#include "LumenPCH.h"
#include "Framework/Camera.h"
#include "Framework/CommandBuffer.h"
#include "Framework/Pipeline.h"
#include "Framework/Scene.h"
#include "Framework/Shader.h"
#include "Framework/Texture.h"
#include "Framework/Utils.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"
#include "Framework/gltfscene.hpp"
#include "shaders/commons.h"
#include <glm/glm.hpp>

enum Integrator {
	PT,
	BDPT,
	PPM,
	PIPELINE_COUNT = 4 // +1 from 2nd PPM pass
};

class RTScene : public Scene {
public:
	RTScene(int width, int height, bool debug);
	void init(Window*) override;
	void update() override;
	void cleanup() override;

	static RTScene* instance;
	inline static RTScene* get() { return instance; }
	bool resized = false;

private:
	void init_scene();
	void create_offscreen_resources();
	void create_graphics_pipeline();
	void create_descriptors();
	void update_descriptors();
	void create_uniform_buffers();
	void update_uniform_buffers();
	void init_imgui();
	void create_blas();
	void create_tlas();
	void create_rt_descriptors();
	void create_rt_pipelines();
	void create_compute_pipelines();
	void create_post_descriptor();
	void create_post_pipeline();
	void update_post_desc_set();

	void render(uint32_t idx);
	double draw_frame();
	struct MaterialPushConst {
		glm::vec4 base_color_factor;
		glm::vec3 emissive_factor;
		int texture_id = -1;
	} material_push_const;
	struct ModelPushConst {
		glm::mat4 transform;
	} model_push_const;
	GraphicsPipelineSettings gfx_pipeline_settings = {};

	VkDescriptorSetLayout desc_set_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout rt_desc_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout post_desc_layout = VK_NULL_HANDLE;

	VkDescriptorPool gfx_desc_pool = VK_NULL_HANDLE;
	VkDescriptorPool rt_desc_pool = VK_NULL_HANDLE;
	VkDescriptorPool post_desc_pool = VK_NULL_HANDLE;

	VkPipelineLayout gfx_pipeline_layout = VK_NULL_HANDLE;
	VkPipelineLayout post_pipeline_layout = VK_NULL_HANDLE;

	std::vector<VkDescriptorSet> uniform_descriptor_sets{};
	VkDescriptorSet rt_desc_set;
	VkDescriptorSet post_desc_set;

	std::unique_ptr<Pipeline> gfx_pipeline = nullptr;
	std::unique_ptr<Pipeline> post_pipeline = nullptr;
	std::unique_ptr<Pipeline> gather_pipeline = nullptr;
	std::unique_ptr<Pipeline> min_pipeline = nullptr;
	std::unique_ptr<Pipeline> min_reduce_pipeline = nullptr;
	std::unique_ptr<Pipeline> max_pipeline = nullptr;
	std::unique_ptr<Pipeline> max_reduce_pipeline = nullptr;
	std::unique_ptr<Pipeline> calc_bounds_pipeline = nullptr;
	std::unique_ptr<Pipeline> update_pipeline = nullptr;
	std::vector<std::unique_ptr<Pipeline>> rt_pipelines;

	VkRenderPass offscreen_renderpass;

	VkFramebuffer offscreen_framebuffer;

	Texture2D offscreen_img;
	Texture2D offscreen_tmp_img;
	Texture2D offscreen_depth;

	VkSampler texture_sampler;
	std::vector<Texture2D> textures;

	//std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
	std::unique_ptr<Camera> camera = nullptr;
	// MeshLoader loader;
	Window* window;
	bool initialized = false;
	bool rt_initialized = false;
	float cpu_avg_time = 0;
	int cnt = 0;
	VkDescriptorPool imgui_pool;
	SceneUBO scene_ubo{};
	GltfScene gltf_scene;
	Buffer vertex_buffer;
	Buffer normal_buffer;
	Buffer uv_buffer;
	Buffer index_buffer;
	Buffer materials_buffer;
	Buffer prim_lookup_buffer;
	Buffer scene_desc_buffer;
	Buffer scene_ubo_buffer;
	Buffer light_vis_buffer;
	// BDPT buffers
	Buffer light_path_buffer;
	Buffer camera_path_buffer;
	Buffer path_backup_buffer;
	Buffer color_storage_buffer;
	// SPPM buffers
	Buffer sppm_data_buffer;
	Buffer atomic_data_buffer;
	Buffer photon_buffer;
	Buffer residual_buffer;
	Buffer residual2_buffer;
	Buffer counter_buffer;
	Buffer hash_buffer;


	Buffer mesh_lights_buffer;
	std::vector<MeshLight> lights;
	Buffer light_matrices_buffer;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	PushConstantRay pc_ray;
	
	std::array<VkPipeline, 3> integrator_pipelines;
	bool updated = false;
};
