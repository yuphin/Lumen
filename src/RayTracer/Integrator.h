#pragma once
#include "Framework/LumenInstance.h"
#include "Framework/Texture.h"
#include "shaders/commons.h"
#include "LumenScene.h"
#include "Framework/RenderGraph.h"
#include "Framework/DynamicResourceManager.h"
#include "Framework/PersistentResourceManager.h"
class Integrator {
   public:
	Integrator(LumenScene* lumen_scene, const vk::BVH& tlas)
		: lumen_scene(lumen_scene), tlas(tlas) {}
	virtual void init();
	virtual void render(){};
	virtual bool gui();
	virtual bool update();
	virtual void destroy();
	vk::Texture* output_tex;
	bool updated = false;
	uint frame_num = 0;

   protected:
	void update_uniform_buffers();
	SceneUBO scene_ubo{};
	// uint32_t Window::width() = UINT_MAX;
	// uint32_t Window::height()  = UINT_MAX;
	LumenScene* lumen_scene = nullptr;
	vk::Buffer* scene_ubo_buffer = nullptr;
	const vk::BVH& tlas;
};
