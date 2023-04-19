#pragma once
#include "Integrator.h"
class Path : public Integrator {
   public:
	Path(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), PathConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	PCPath pc_ray{};
	PathConfig* config;
};
