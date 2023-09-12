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
	Buffer radiance_query_buffer_ping;
	Buffer radiance_target_buffer_ping;
	Buffer radiance_query_buffer_pong;
	Buffer radiance_target_buffer_pong;
	Buffer sample_count_buffer;
	float* radiance_query_addr_cuda = nullptr;
	cudaExternalMemory_t radiance_query_mem_cuda;
	float* radiance_target_addr_cuda = nullptr;
	cudaExternalMemory_t radiance_target_mem_cuda;
	uint32_t* sample_count_addr_cuda = nullptr;
	cudaExternalMemory_t sample_count_mem_cuda;
	int max_samples_count = -1;
};
