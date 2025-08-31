#pragma once
#include "Integrator.h"
#include "shaders/integrators/path/path_commons.h"
class Path final : public Integrator {
   public:
	Path(const vk::BVH& tlas) : Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;
	virtual bool gui() override;

   private:
	PCPath pc_ray{};
	u32 path_length = 0;
	bool direct_lighting = true;
};
