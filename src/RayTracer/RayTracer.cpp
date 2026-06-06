#include "Framework/RenderGraph.h"
#include "Framework/GPUQueryManager.h"
#include "Framework/ImageUtils.h"
#include <tinyexr.h>
#include "RayTracer.h"
#include "Integrator.h"
#include "PostFX.h"

namespace ray_tracer {

static bool load_reference = false;
static bool calc_rmse = false;
static bool initialized = false;
static f32 cpu_avg_time = 0;
static i32 cnt = 0;
static Integrator active_integrator;
static PostFX post_fx;
static RTUtilsPC rt_utils_pc;
static vk::Buffer* gt_img_buffer = nullptr;
static vk::Buffer* output_img_buffer = nullptr;
static vk::Buffer* output_img_buffer_cpu = nullptr;
static vk::Buffer* residual_buffer = nullptr;
static vk::Buffer* counter_buffer = nullptr;
static vk::Buffer* rmse_val_buffer = nullptr;
static vk::Buffer* rt_utils_desc_buffer = nullptr;
static vk::Texture* reference_tex = nullptr;
static vk::Texture* target_tex = nullptr;
static clock_t start;
static bool debug = false;
static bool write_exr = false;
static bool has_gt = false;
static bool show_cam_stats = false;
static bool comparison_mode = false;
static bool capture_ref_img = false;
static bool capture_target_img = false;
static bool comparison_img_toggle = false;
static bool img_captured = false;
static bool show_ui = true;
static const bool enable_shader_inference = true;
static const bool use_events = true;
static vk::BVH tlas;
static lm::Array<vk::BVH> blases;
static bool recreate_swapchain = false;
static SceneConfig saved_configs[INTEGRATOR_COUNT];
static bool saved_config_valid[INTEGRATOR_COUNT] = {};
static i32 current_integrator_idx = 0;

static const char* integrator_display_names[INTEGRATOR_COUNT] = {
	"Path", "BDPT", "SPPM", "VCM", "PSSMLT", "SMLT", "VCMMLT", "ReSTIR", "ReSTIR GI", "DDGI", "ReSTIR PT",
	"IR Cache",
};

static const lm::String integrator_config_names[INTEGRATOR_COUNT] = {
	"path", "bdpt", "sppm", "vcm", "pssmlt", "smlt", "vcmmlt", "restir", "restirgi", "ddgi", "restirpt", "ircache",
};

static void init_resources();
static void cleanup_resources();
static f32 draw_frame();
static void render(u32 idx);
static void render_debug_utils();
static bool gui();
static void destroy_accel();

static void reload_shaders() {
	vk::render_graph()->reload_shaders = true;
	vk::render_graph()->shader_cache.clear();
	vk::render_graph()->reload_counter++;
	vk::shader_arena_reset();
}

static void cache_active_config() {
	SceneConfig& config = scene::get()->config;
	saved_configs[config.type] = config;
	saved_config_valid[config.type] = true;
}

static void select_integrator_config(IntegratorType type, const SceneCommon& common) {
	if (saved_config_valid[type]) {
		scene::get()->config = saved_configs[type];
		scene::get()->config.common = common;
	} else {
		scene::config_init(integrator_config_names[type], common);
	}
	scene::get()->config.type = type;
	scene::get()->config.common.integrator_name = integrator_config_names[type];
}

void init(bool use_debug, i32 argc, char* argv[]) {
	debug = use_debug;
	lm::String scene_name = CSTR("scenes/caustics.scene");
	for (i32 i = 0; i < argc; i++) {
		// +1 for null terminator
		lm::String arg_str = lm::String(argv[i], strlen(argv[i]) + 1);
		if (lm::str_ends_with(arg_str, CSTR(".scene"))) {
			scene_name = arg_str;
		}
	}
	srand((u32)time(NULL));
	Window::add_key_callback([](KeyInput key, KeyAction action) {
		if (Window::is_key_down(KeyInput::KEY_F1)) {
			show_ui = !show_ui;
		}
		if (Window::is_key_down(KeyInput::KEY_F10)) {
			write_exr = true;
		} else if (Window::is_key_down(KeyInput::KEY_F11)) {
			comparison_mode ^= true;
		} else if (Window::is_key_down(KeyInput::KEY_F5)) {
			reload_shaders();
			active_integrator.updated = true;
		} else if (Window::is_key_down(KeyInput::KEY_F6)) {
			capture_ref_img = true;
		} else if (Window::is_key_down(KeyInput::KEY_F7)) {
			capture_target_img = true;
		} else if (comparison_mode && Window::is_key_down(KeyInput::KEY_LEFT)) {
			comparison_img_toggle = false;
		} else if (comparison_mode && Window::is_key_down(KeyInput::KEY_RIGHT)) {
			comparison_img_toggle = true;
		}
	});

	// Init with ray tracing extensions
	vk::add_device_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	vk::add_device_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	vk::add_device_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	vk::add_device_extension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
	vk::add_device_extension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
	vk::add_device_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
	vk::add_device_extension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
	vk::add_device_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);

	vk::context().vsync_enabled = true;

	vk::init(debug);
	initialized = true;

	// Enable shader reflections for the render graph
	vk::render_graph()->settings.shader_inference = enable_shader_inference;
	// Event based synchronization instead of barriers
	vk::render_graph()->settings.use_events = use_events;

	scene::load(scene_name);
	current_integrator_idx = i32(scene::get()->config.type);
	cache_active_config();
	active_integrator.tlas = &tlas;
	integrator::set_type(&active_integrator, scene::get()->config.type);
	integrator::init(&active_integrator);
	if (!tlas.accel) {
		integrator::create_accel(&active_integrator, &tlas, &blases);
	}
	post_fx.init();
	init_resources();
}

static void init_resources() {
	u32 viewport_size = Window::width() * Window::height();
	output_img_buffer =
		prm::get_buffer({.name = CSTR("Output Image Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = viewport_size * 4 * 4});

	output_img_buffer_cpu =
		prm::get_buffer({.name = CSTR("Output Image CPU"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
						 .size = viewport_size * 4 * 4});

	residual_buffer =
		prm::get_buffer({.name = CSTR("RMSE Residual"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = viewport_size * 4});

	counter_buffer =
		prm::get_buffer({.name = CSTR("RMSE Counter"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(i32)});

	rmse_val_buffer =
		prm::get_buffer({.name = CSTR("RMSE Value"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
						 .size = sizeof(f32)});
	auto texture_desc = vk::TextureDesc{.name = CSTR("Reference Texture"),
										.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
												 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
										.dimensions = {Window::width(), Window::height(), 1},
										.format = VK_FORMAT_R32G32B32A32_SFLOAT,
										.initial_layout = VK_IMAGE_LAYOUT_GENERAL};
	reference_tex = prm::get_texture(texture_desc);
	texture_desc.name = CSTR("Target Texture");
	target_tex = prm::get_texture(texture_desc);

	RTUtilsDesc rt_utils_desc;
	if (load_reference) {
		// Load the ground truth image
		i32 width, height;
		f32* data = ImageUtils::load_exr("out.exr", width, height);
		if (!data) {
			LUMEN_ERROR("Could not load the reference image");
		}
		gt_img_buffer =
			prm::get_buffer({.name = CSTR("Ground Truth Image"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = Window::width() * Window::height() * 4 * sizeof(f32),
							 .data = data});
		rt_utils_desc.gt_img_addr = gt_img_buffer->device_address();
		free(data);
	}

	rt_utils_desc.out_img_addr = output_img_buffer->device_address();
	rt_utils_desc.residual_addr = residual_buffer->device_address();
	rt_utils_desc.counter_addr = counter_buffer->device_address();
	rt_utils_desc.rmse_val_addr = rmse_val_buffer->device_address();

	rt_utils_desc_buffer =
		prm::get_buffer({.name = CSTR("RT Utils Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(RTUtilsDesc),
						 .data = &rt_utils_desc});

	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, out_img_addr, output_img_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, residual_addr, residual_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, counter_addr, counter_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, rmse_val_addr, rmse_val_buffer, vk::render_graph());
}

static void cleanup_resources() {
	vk::Buffer** buffers[] = {&output_img_buffer, &output_img_buffer_cpu, &residual_buffer, &counter_buffer,
							 &rmse_val_buffer, &rt_utils_desc_buffer, &gt_img_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
	vk::Texture** textures[] = {&reference_tex, &target_tex};
	for (vk::Texture** texture : textures) {
		prm::remove(*texture);
		*texture = nullptr;
	}
}

void update() {
	f32 frame_time = draw_frame();
	cpu_avg_time = (1.0f - 1.0f / (cnt)) * cpu_avg_time + frame_time / (f32)cnt;
	cpu_avg_time = 0.95f * cpu_avg_time + 0.05f * frame_time;
	integrator::update(&active_integrator);
	active_integrator.updated = false;
#if 0
	char* stats = nullptr;
	vmaBuildStatsString(vk::context().allocator, &stats, VK_TRUE);
	printf("Stats--\n");
	LUMEN_TRACE("%s", stats);
#endif
}

static void render(u32 i) {
	integrator::render(&active_integrator);
	vk::Texture* input_tex = nullptr;
	if (comparison_mode && img_captured) {
		input_tex = comparison_img_toggle ? target_tex : reference_tex;
	} else {
		input_tex = active_integrator.output_tex;
	}
	post_fx.render(input_tex, vk::swapchain_images()[i]);
	render_debug_utils();

	auto cmdbuf = vk::context().command_buffers[i];
	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vk::check(vkBeginCommandBuffer(cmdbuf, &begin_info));
	vk::render_graph()->run(cmdbuf);
	vk::check(vkEndCommandBuffer(cmdbuf));
}

static void render_debug_utils() {
	if (write_exr) {
		vk::render_graph()->current_pass().copy(active_integrator.output_tex, output_img_buffer_cpu);
	} else if (capture_ref_img) {
		vk::render_graph()->current_pass().copy(active_integrator.output_tex, reference_tex);

	} else if (capture_target_img) {
		vk::render_graph()->current_pass().copy(active_integrator.output_tex, target_tex);
	}

	if (capture_ref_img || capture_target_img) {
		img_captured = true;
		capture_ref_img = false;
		capture_target_img = false;
	}

	if (calc_rmse && has_gt) {
		auto op_reduce = [&](const lm::String& op_name, const lm::String& op_shader_name, const lm::String& reduce_name,
							 const lm::String& reduce_shader_name) {
			u32 num_wgs = u32((Window::width() * Window::height() + 1023) / 1024);
			vk::render_graph()
				->add_compute(op_name, {.shader = vk::Shader(op_shader_name), .dims = {num_wgs, 1, 1}})
				.push_constants(&rt_utils_pc)
				.bind(rt_utils_desc_buffer)
				.zero({residual_buffer, counter_buffer});
			while (num_wgs != 1) {
				vk::render_graph()
					->add_compute(reduce_name, {.shader = vk::Shader(reduce_shader_name), .dims = {num_wgs, 1, 1}})
					.push_constants(&rt_utils_pc)
					.bind(rt_utils_desc_buffer);
				num_wgs = (num_wgs + 1023) / 1024;
			}
		};
		vk::render_graph()->current_pass().copy(active_integrator.output_tex, output_img_buffer);
		// Calculate RMSE
		op_reduce(CSTR("OpReduce: RMSE"), CSTR("src/shaders/rmse/calc_rmse.comp"), CSTR("OpReduce: Reduce RMSE"),
				  CSTR("src/shaders/rmse/reduce_rmse.comp"));
		vk::render_graph()
			->add_compute(CSTR("Calculate RMSE"),
						  {.shader = vk::Shader(CSTR("src/shaders/rmse/output_rmse.comp")), .dims = {1, 1, 1}})
			.push_constants(&rt_utils_pc)
			.bind(rt_utils_desc_buffer);
	}
}

static bool gui() {
	util::Slice<GPUQueryManager::TimestampData> query_results = GPUQueryManager::get();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	ImGui::Text("General settings:");
	ImGui::PopStyleColor();
	ImGui::Text("Frame %d time (CPU) %.2f ms ( %.2f FPS )", active_integrator.frame_num, cpu_avg_time,
				1000 / cpu_avg_time);
	double frame_time_gpu_ms = (GPUQueryManager::get_total_elapsed()) * 1e-6;
	if (frame_time_gpu_ms > 0) {
		ImGui::Text("Frame time (GPU) %.2f ms", frame_time_gpu_ms);
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
		ImGui::Text("Individual GPU timings:");
		ImGui::PopStyleColor();
		for (u64 i = 0; i < query_results.size; i++) {
			const GPUQueryManager::TimestampData& data = query_results[i];
			GPUQueryManager::TimestampData* parent = data.parent;
			u32 scope = 0;
			while (parent != nullptr) {
				scope++;
				parent = parent->parent;
			}
			double elapsed_ms = GPUQueryManager::get_elapsed(data) * 1e-6;
			std::string indent(scope * 2, ' ');	 // Indent by 2 spaces per depth
			std::string indented_text = std::format("{}{:.2f} ms: {}", indent, elapsed_ms, data.name);
			ImGui::Text("%s", indented_text.c_str());
		}
	}

	ImGui::Text("GPU Memory Usage: %.2f MB", vk::get_memory_usage(vk::context().physical_device) / (1024.0f * 1024.0f));
	u64 arena_total_used_bytes = 0;
	u64 arena_total_allocated_bytes = 0;
	lm::get_all_arena_stats(arena_total_used_bytes, arena_total_allocated_bytes);
	ImGui::Text("CPU Arena Usage: %.2f MB Allocated, %.2f MB Used", arena_total_allocated_bytes / (1024.0f * 1024.0f),
				arena_total_used_bytes / (1024.0f * 1024.0f));
	bool updated = false;
	ImGui::Checkbox("Show camera statistics", &show_cam_stats);
	if (show_cam_stats) {
		const lm::Camera& camera = scene::get()->camera;
		ImGui::Text("X - Right, Y - Up, -Z - Forward");
		ImGui::Text("Camera position: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
		ImGui::Text("Camera rotation (degrees): X:%.2f Y:%.2f Z:%.2f", camera.rotation.x, camera.rotation.y,
					camera.rotation.z);
		ImGui::Text("Camera direction:  %.2f %.2f %.2f", camera.direction.x, camera.direction.y, camera.direction.z);
		if (ImGui::Button("Copy camera data to clipboard")) {
			std::string cam_pos_str = std::format(
				"    \"position\": "
				"[{:.2f},{:.2f},{:.2f}],\n    \"rotation\":[{:.2f},{:.2f},{:.2f}],\n    \"dir\":[{:.2f},{:.2f},{:.2f}]",
				camera.position.x, camera.position.y, camera.position.z, camera.rotation.x, camera.rotation.y,
				camera.rotation.z, camera.direction.x, camera.direction.y, camera.direction.z);
			glfwSetClipboardString(Window::get()->window_handle, cam_pos_str.c_str());
		}
	}
	if (ImGui::Checkbox("Enable VSync", &vk::context().vsync_enabled)) {
		recreate_swapchain = true;
	}
	if (ImGui::Button("Reload shaders (F5)")) {
		reload_shaders();
		updated |= true;
	}
	ImGui::Checkbox("Comparison mode (F11)", &comparison_mode);
	if (comparison_mode && img_captured) {
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
		const char* texts[] = {"Showing: Reference Image", "Showing: Target Image"};
		ImGui::Text("%s\n", texts[u32(comparison_img_toggle)]);
		ImGui::PopStyleColor();
	}
	if (ImGui::Button("Capture reference image (F6)")) {
		capture_ref_img = true;
	}
	if (ImGui::Button("Capture target image (F7)")) {
		capture_target_img = true;
	}

	SceneConfig& config = scene::get()->config;
	if (ImGui::BeginCombo("Select Integrator", integrator_display_names[current_integrator_idx])) {
		for (i32 n = 0; n < INTEGRATOR_COUNT; n++) {
			const bool selected = current_integrator_idx == n;
			if (ImGui::Selectable(integrator_display_names[n], selected)) {
				current_integrator_idx = n;
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (current_integrator_idx != i32(config.type)) {
		updated = true;
		vkDeviceWaitIdle(vk::context().device);
		const bool was_custom_accel = config.type == INTEGRATOR_DDGI;
		SceneCommon common = config.common;
		cache_active_config();
		integrator::destroy(&active_integrator, /*resize=*/false);
		IntegratorType new_type = IntegratorType(current_integrator_idx);
		select_integrator_config(new_type, common);
		GPUQueryManager::reset_data();
		integrator::set_type(&active_integrator, new_type);
		const bool is_custom_accel = new_type == INTEGRATOR_DDGI;
		integrator::init(&active_integrator);
		if (was_custom_accel || is_custom_accel) {
			destroy_accel();
			integrator::create_accel(&active_integrator, &tlas, &blases);
		}
	}
	return updated;
}

static f32 draw_frame() {
	if (cnt == 0) {
		start = clock();
	}

	auto t_begin = glfwGetTime() * 1000;
	bool updated = false;
	u32 image_idx = vk::prepare_frame();
	if (image_idx == UINT32_MAX) {
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		return (f32)t_diff;
	}
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	active_integrator.updated |= updated;
	if (show_ui) {
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
		ImGui::Begin("Debug (F1 to hide)", &show_ui);
		bool gui_updated = gui();
		gui_updated |= integrator::gui(&active_integrator);
		gui_updated |= post_fx.gui();
		static bool show_imgui_demo = false;
		if (ImGui::Button("Show ImGui Demo")) {
			show_imgui_demo = !show_imgui_demo;
		}
		if (show_imgui_demo) {
			ImGui::ShowDemoWindow(&show_imgui_demo);
		}
		ImGui::End();
		active_integrator.updated |= gui_updated;
	}

	render(image_idx);
	VkResult result = vk::submit_frame(image_idx);
	vk::render_graph()->reset();
	vk::render_graph()->reload_shaders = false;
	vk::render_graph()->dirty_pass_encountered = false;
	if (result != VK_SUCCESS) {
		Window::update_window_size();
		cleanup_resources();
		integrator::destroy(&active_integrator, /*resize=*/true);
		post_fx.destroy();

		integrator::init(&active_integrator);
		post_fx.init();
		init_resources();
		active_integrator.updated = true;
	}

	if (recreate_swapchain) {
		vk::recreate_swap_chain();
		recreate_swapchain = false;
	}

	auto now = clock();
	auto diff = ((f32)now - start);

	if (write_exr) {
		write_exr = false;
		ImageUtils::save_exr((f32*)vk::buffer_map(output_img_buffer_cpu), Window::width(), Window::height(), "out.exr");
		vk::buffer_unmap(output_img_buffer_cpu);
	}
	bool time_limit = (abs(diff / CLOCKS_PER_SEC - 5)) < 0.1;
	calc_rmse = time_limit;

	if (calc_rmse && has_gt) {
		f32 rmse = *(f32*)vk::buffer_map(rmse_val_buffer);
		vk::buffer_unmap(rmse_val_buffer);
		LUMEN_TRACE("RMSE: %f", rmse * 1e6);
		start = now;
	}
	auto t_end = glfwGetTime() * 1000;
	auto t_diff = t_end - t_begin;
	cnt++;
	return (f32)t_diff;
}

static void destroy_accel() {
	tlas.destroy();
	for (vk::BVH& blas : blases) {
		blas.destroy();
	}
	blases.clear();
}

void cleanup() {
	vkDeviceWaitIdle(vk::context().device);
	if (initialized) {
		cleanup_resources();
		integrator::destroy(&active_integrator, /*resize=*/false);
		post_fx.destroy();
		scene::destroy();
		destroy_accel();
		vk::destroy_imgui();
		vk::cleanup();
		initialized = false;
	}
}

}  // namespace ray_tracer
