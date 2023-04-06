#pragma once
#include "LumenPCH.h"
#include "Framework/LumenInstance.h"
#include "Framework/ImageUtils.h"
#include "Path.h"
#include "BDPT.h"
#include "SPPM.h"
#include "VCM.h"
#include "PSSMLT.h"
#include "SMLT.h"
#include "VCMMLT.h"
#include "ReSTIR.h"
#include "ReSTIRGI.h"
#include "DDGI.h"

class RayTracer : public LumenInstance {
   public:
	RayTracer(int width, int height, bool debug, int, char*[]);
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

	void init_resources();
	void parse_args(int argc, char* argv[]);
	float draw_frame();
	void render(uint32_t idx);
	bool initialized = false;
	bool rt_initialized = false;
	float cpu_avg_time = 0;
	int cnt = 0;
	std::unique_ptr<Integrator> integrator;

	PushConstantPost pc_post_settings;
	Settings settings;
	Buffer gt_img_buffer;
	Buffer output_img_buffer;
	Buffer output_img_buffer_cpu;
	Buffer post_desc_buffer;
	Buffer residual_buffer;
	Buffer counter_buffer;
	Buffer rmse_val_buffer;

	Buffer fft_buffers[2];
	Buffer fft_cpu_buffers[2];
	PostPC post_pc;
	FFTPC fft_pc;
	std::string scene_name;
	LumenScene scene;

	std::vector<glm::vec2> fft_arr;

	clock_t start;
	bool write_exr = false;
	bool has_gt = false;
	bool show_cam_stats = false;
	const uint32_t FFT_SIZE = 2048;

	Texture2D input_img;
	Texture2D fft_img;
	Texture2D kernel_ping;
	Texture2D kernel_pong;
	VkSampler img_sampler;

};
