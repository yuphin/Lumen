#pragma once
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
#include "Framework/Base/String.h"

class RayTracer {
   public:
	void init(bool use_debug, i32 argc, char* argv[]);
	void update();
	void cleanup();
	static RayTracer* instance;
	inline static RayTracer* get() { return instance; }
	bool resized = false;

   private:
	void init_resources();
	void cleanup_resources();
	f32 draw_frame();
	void render(u32 idx);
	void render_debug_utils();
	void create_integrator(i32 integrator_idx);
	bool gui();
	void destroy_accel();
	bool initialized = false;
	f32 cpu_avg_time = 0;
	i32 cnt = 0;
	std::unique_ptr<Integrator> integrator;
	PostFX post_fx;

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

	lm::String scene_name;
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
	bool show_ui = true;

	const bool enable_shader_inference = true;
	const bool use_events = true;
	vk::BVH tlas;
	std::vector<vk::BVH> blases;
	bool recreate_swapchain = false;
};
