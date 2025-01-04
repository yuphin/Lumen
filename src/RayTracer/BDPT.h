#pragma once
#include "Integrator.h"
#include "shaders/integrators/bdpt/bdpt_commons.h"

class BDPT final : public Integrator {
   public:
	BDPT(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), BDPTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;

   private:
	PCBDPT pc_ray{};
	vk::Buffer* light_path_buffer;
	vk::Buffer* camera_path_buffer;
	vk::Buffer* color_storage_buffer;
	BDPTConfig* config;
};
