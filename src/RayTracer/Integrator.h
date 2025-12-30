#pragma once
#include "Framework/Texture.h"
#include "shaders/commons.h"
#include "LumenScene.h"
#include "Framework/DynamicResourceManager.h"
#include "Framework/PersistentResourceManager.h"
#include "Framework/Base/String.h"
#include "Framework/Base/OS.h"
#include "Framework/Base/Memory.h"
#include "Framework/Base/HashMap.h"

class Integrator {
   public:
	Integrator(const vk::BVH& tlas) : tlas(tlas) {}
	virtual void init();
	virtual void render() = 0;
	virtual bool gui();
	virtual bool update();
	virtual void destroy(bool resize);
	virtual void create_accel(vk::BVH& tlas, lm::Array<vk::BVH>& blases);
	vk::Texture* output_tex;
	bool updated = false;
	uint frame_num = 0;

   protected:
	void update_uniform_buffers();
	SceneUBO scene_ubo{};
	vk::Buffer* scene_ubo_buffer = nullptr;
	const vk::BVH& tlas;
	scene::Scene* lumen_scene = nullptr;
	lm::Arena* arena = nullptr;
};
