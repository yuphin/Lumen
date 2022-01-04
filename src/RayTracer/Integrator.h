#pragma once
#include "LumenPCH.h"
#include "Framework/Camera.h"
#include "Framework/CommandBuffer.h"
#include "Framework/Pipeline.h"
#include "Framework/LumenInstance.h"
#include "Framework/Shader.h"
#include "Framework/Texture.h"
#include "Framework/Utils.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"
#include "Framework/GltfScene.hpp"
#include "Framework/LumenScene.h"
#include "shaders/commons.h"
class Integrator {
public:
	Integrator(LumenInstance* instance, const SceneConfig& config) : 
		instance(instance), config(config) {}
	virtual void init();
	virtual void render() = 0;
	virtual bool update() = 0;
	virtual void reload() = 0;
	virtual void destroy();
	Texture2D output_tex;
	std::unique_ptr<Camera> camera = nullptr;
	bool updated = false;
protected:
	void update_uniform_buffers();
	SceneUBO scene_ubo{};
	Buffer vertex_buffer;
	Buffer normal_buffer;
	Buffer uv_buffer;
	Buffer index_buffer;
	Buffer materials_buffer;
	Buffer prim_lookup_buffer;
	Buffer scene_desc_buffer;
	Buffer scene_ubo_buffer;
	Buffer mesh_lights_buffer;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
	LumenInstance* instance;
	LumenScene lumen_scene;
	std::vector<MeshLight> lights;
	VkSampler texture_sampler;
	std::vector<Texture2D> textures;
	SceneConfig config;
private:
};

