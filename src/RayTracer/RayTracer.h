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
#include "PostFX.h"

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

	void assign_integrator(IntegratorType type);
	void init_resources();
	void parse_args(int argc, char* argv[]);
	float draw_frame();
	void render(uint32_t idx);
	bool initialized = false;
	bool rt_initialized = false;
	float cpu_avg_time = 0;
	int cnt = 0;
	std::unique_ptr<Integrator> integrator;
	PostFX post_fx;

	RTUtilsDesc rt_utils_desc;
	RTUtilsPC rt_utils_pc;

	Buffer gt_img_buffer;
	Buffer output_img_buffer;
	Buffer output_img_buffer_cpu;
	Buffer residual_buffer;
	Buffer counter_buffer;
	Buffer rmse_val_buffer;
	Buffer rt_utils_desc_buffer;


	Buffer fft_buffers[2];
	Buffer fft_cpu_buffers[2];
	std::string scene_name;
	LumenScene scene;

	clock_t start;
	bool write_exr = false;
	bool has_gt = false;
	bool show_cam_stats = false;
};
