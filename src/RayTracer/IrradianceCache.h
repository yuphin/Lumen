#pragma once
#include "Integrator.h"
#include "shaders/integrators/irradiance_cache/ir_commons.h"
using namespace IRCache;
class IrradianceCache : public Integrator {
   public:
	IrradianceCache(const vk::BVH& tlas)
		: Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;
	virtual bool gui() override;

   private:
	vk::Buffer* gbuffer;
	vk::Buffer* transformations_buffer;
	vk::Buffer* hash_cells_buffer;
	PCIRCache pc{};
	PathConfig* config;
	bool direct_lighting = false;
	float min_cell_size = 0.5f;
	float desired_px_per_cell = 4.0f;
};
