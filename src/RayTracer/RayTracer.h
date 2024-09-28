#pragma once
#include "LumenPCH.h"
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
#include "Framework/Window.h"

class RayTracer {
   public:
	RayTracer(bool debug, int, char*[]);
	void init();
	void update();
	void cleanup();
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
	void destroy_accel();
	bool initialized = false;
	bool rt_initialized = false;
	float cpu_avg_time = 0;
	int cnt = 0;
	std::unique_ptr<Integrator> integrator;
	PostFX post_fx;

	RTUtilsDesc rt_utils_desc;
	RTUtilsPC rt_utils_pc;

	vk::Buffer* gt_img_buffer;
	vk::Buffer* output_img_buffer;
	vk::Buffer* output_img_buffer_cpu;
	vk::Buffer* residual_buffer;
	vk::Buffer* counter_buffer;
	vk::Buffer* rmse_val_buffer;
	vk::Buffer* rt_utils_desc_buffer;

	vk::Texture* reference_tex;
	vk::Texture* target_tex;

	vk::Buffer* fft_buffers[2];
	vk::Buffer* fft_cpu_buffers[2];
	std::string scene_name;
	LumenScene scene;

	clock_t start;
	bool debug = false;
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
	vk::BVH tlas;
	std::vector<vk::BVH> blases;
};
