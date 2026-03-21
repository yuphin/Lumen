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
	vk::Buffer* surfel_spawn_list_buffer;
	vk::Buffer* surfel_spawn_count_buffer;
	vk::Buffer* surfel_pool_buffer;
	vk::Buffer* surfel_free_stack_counter_buffer;
	vk::Buffer* surfel_free_stack_buffer;
	// Grid
	vk::Buffer* grid_cell_counts_buffer;
	vk::Buffer* grid_cell_offsets_buffer;
	vk::Buffer* grid_cell_stacks_buffer;
	vk::Buffer* grid_cell_indices_buffer;

	lm::FixedArray<vk::Buffer*> block_sums;

	PCIRCache pc{};
	PathConfig* config;
	bool direct_lighting = false;
	bool debug_mode = true;
	u32 total_frame_idx = 0;
};
