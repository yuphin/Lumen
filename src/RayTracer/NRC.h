#pragma once
#include "Integrator.h"
#include "shaders/integrators/nrc/nrc_commons.h"
class NRC : public Integrator {
   public:
	NRC(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), NRCConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	PCNRC pc_ray{};
	NRCConfig* config;
	Buffer test_buffer2;
	VkBuffer test_buffer;
	VkDeviceMemory test_memory;
	float* cuda_mem_ptr = nullptr;
	cudaExternalMemory_t cuda_mem;
};
