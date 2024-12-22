#pragma once
#include "Integrator.h"
#include "shaders/integrators/path/path_commons.h"
class Path final : public Integrator {
   public:
	Path(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), PathConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
	virtual bool gui() override;

   private:
	PCPath pc_ray{};
	PathConfig* config;
	uint32_t path_length = 0;
	bool direct_lighting = true;
};
