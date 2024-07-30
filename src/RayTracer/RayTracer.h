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
#include "ReSTIRPT.h"
#include "DDGI.h"
#include "PostFX.h"

class RayTracer : public lumen::LumenInstance {
   public:
	RayTracer(int width, int height, bool debug, int, char*[]);
	void init(Window*) override;
	void update() override;
	void cleanup() override;
	static RayTracer* instance;
	inline static RayTracer* get() { return instance; }
	bool resized = false;

   private:
	void init_resources();
	void cleanup_resources();
	void parse_args(int argc, char* argv[]);
	float draw_frame();
	void render(uint32_t idx);
	void render_debug_utils();
	void create_integrator(int integrator_idx);
	bool gui();
	bool initialized = false;
	bool rt_initialized = false;
	float cpu_avg_time = 0;
	int cnt = 0;
	std::unique_ptr<Integrator> integrator;
	PostFX post_fx;

	RTUtilsDesc rt_utils_desc;
	RTUtilsPC rt_utils_pc;

	lumen::Buffer gt_img_buffer;
	lumen::Buffer output_img_buffer;
	lumen::Buffer output_img_buffer_cpu;
	lumen::Buffer residual_buffer;
	lumen::Buffer counter_buffer;
	lumen::Buffer rmse_val_buffer;
	lumen::Buffer rt_utils_desc_buffer;

	lumen::Texture2D reference_tex;
	lumen::Texture2D target_tex;

	lumen::Buffer fft_buffers[2];
	lumen::Buffer fft_cpu_buffers[2];
	std::string scene_name;
	LumenScene scene;

	clock_t start;
	bool write_exr = false;
	bool has_gt = false;
	bool show_cam_stats = false;

	bool comparison_mode = false;
	bool capture_ref_img = false;
	bool capture_target_img = false;
	bool comparison_img_toggle = false;
	bool img_captured = false;

	const bool enable_shader_inference = true;
	const bool use_events = true;
};
