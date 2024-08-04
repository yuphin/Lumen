#pragma once
#include "Integrator.h"
#include "shaders/integrators/bdpt/bdpt_commons.h"

class BDPT : public Integrator {
   public:
	BDPT(lumen::LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), BDPTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	PCBDPT pc_ray{};
	lumen::BufferOld light_path_buffer;
	lumen::BufferOld camera_path_buffer;
	lumen::BufferOld color_storage_buffer;
	BDPTConfig* config;
};
