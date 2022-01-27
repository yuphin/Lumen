#pragma once
#include "LumenPCH.h"
#include "Framework/LumenInstance.h"
#include "Path.h"
#include "BDPT.h"
#include "SPPM.h"
#include "VCM.h"
#include "PSSMLT.h"
#include "SMLT.h"
#include "VCMMLT.h"
#include "ReSTIR.h"
#include "ReSTIRGI.h"
class RayTracer : public LumenInstance {
public:
	RayTracer(int width, int height, bool debug);
	void init(Window*) override;
	void update() override;
	void cleanup() override;
	static RayTracer* instance;
	inline static RayTracer* get() { return instance; }
	bool resized = false;
private:
	struct Settings {
		bool enable_tonemapping = false;
	};
	void render(uint32_t idx);
	float draw_frame();
	void create_post_descriptor();
	void update_post_desc_set();
	void create_post_pipeline();
	void init_imgui();
	bool initialized = false;
	bool rt_initialized = false;
	float cpu_avg_time = 0;
	int cnt = 0;
	std::unique_ptr<Integrator> integrator;

	VkDescriptorSetLayout post_desc_layout = 0;
	VkDescriptorPool post_desc_pool = 0;
	VkDescriptorPool imgui_pool = 0;
	VkPipelineLayout post_pipeline_layout = 0;
	VkDescriptorSet post_desc_set = 0;
	std::unique_ptr<Pipeline> post_pipeline;

	PushConstantPost pc_post_settings;
	Settings settings;

};

