#pragma once
#include "../LumenPCH.h"
#include "Framework/Camera.h"
#include "Framework/CommandBuffer.h"
#include "Framework/Pipeline.h"
#include "Framework/LumenInstance.h"
#include "Framework/Shader.h"
#include "Framework/Texture.h"
#include "Framework/VkUtils.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"
#include "Framework/GltfScene.hpp"
#include "shaders/commons.h"
#include "LumenScene.h"
class Integrator {
   public:
	Integrator(lumen::LumenInstance* instance, LumenScene* lumen_scene) : instance(instance), lumen_scene(lumen_scene) {}
	virtual void init();
	virtual void render() {};
	virtual bool gui();
	virtual bool update();
	virtual void destroy();
	lumen::Texture2D output_tex;
	std::unique_ptr<lumen::Camera> camera;
	bool updated = false;
	VkSampler texture_sampler;
	uint frame_num = 0;

   protected:
	virtual void update_uniform_buffers();
	SceneUBO scene_ubo{};
	lumen::Buffer vertex_buffer;
	lumen::Buffer compact_vertices_buffer;
	lumen::Buffer index_buffer;
	lumen::Buffer materials_buffer;
	lumen::Buffer prim_lookup_buffer;
	lumen::Buffer scene_desc_buffer;
	lumen::Buffer scene_ubo_buffer;
	lumen::Buffer mesh_lights_buffer;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
	lumen::LumenInstance* instance;
	std::vector<Light> lights;
	std::vector<lumen::Texture2D> scene_textures;
	uint32_t total_light_triangle_cnt = 0;
	float total_light_area = 0;
	LumenScene* lumen_scene;

   private:
	void create_blas();
	void create_tlas();
};
