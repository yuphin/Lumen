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
	bool updated = false;
	uint frame_num = 0;
   protected:
	void update_uniform_buffers();
	SceneUBO scene_ubo{};
	lumen::LumenInstance* instance;
	LumenScene* lumen_scene;
	lumen::Buffer scene_ubo_buffer;
};
