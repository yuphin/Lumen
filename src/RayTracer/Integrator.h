#pragma once
#include "Framework/LumenInstance.h"
#include "Framework/Texture.h"
#include "shaders/commons.h"
#include "LumenScene.h"
#include "Framework/RenderGraph.h"
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
	vk::BVH tlas;
	std::vector<vk::BVH> blases;

	private:
	void create_accel();
};
