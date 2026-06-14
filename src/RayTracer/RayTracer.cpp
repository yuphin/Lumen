#include "Framework/RenderGraph.h"
#include "Framework/GPUQueryManager.h"
#include "Framework/ImageUtils.h"
#include "Framework/ImGuiRenderer.h"
#include "RayTracer.h"
#include "Integrator.h"
#include "PostFX.h"

namespace ray_tracer {

static bool _load_reference = false;
static bool _calc_rmse = false;
static bool _initialized = false;
static f32 _cpu_avg_time = 0;
static i32 _cnt = 0;
static Integrator _active_integrator;
static PostFX _post_fx;
static ImGuiRenderer _imgui_renderer;
static RTUtilsPC _rt_utils_pc;
static vk::Buffer* _gt_img_buffer = nullptr;
static vk::Buffer* _output_img_buffer = nullptr;
static vk::Buffer* _output_img_buffer_cpu = nullptr;
static vk::Buffer* _residual_buffer = nullptr;
static vk::Buffer* _counter_buffer = nullptr;
static vk::Buffer* _rmse_val_buffer = nullptr;
static vk::Buffer* _rt_utils_desc_buffer = nullptr;
static vk::Texture* _reference_tex = nullptr;
static vk::Texture* _target_tex = nullptr;
static f64 _start;
static bool _debug = false;
static bool _write_exr = false;
static bool _has_gt = false;
static bool _show_cam_stats = false;
static bool _comparison_mode = false;
static bool _capture_ref_img = false;
static bool _capture_target_img = false;
static bool _comparison_img_toggle = false;
static bool _img_captured = false;
static bool _show_ui = true;
static const bool _enable_shader_inference = true;
static const bool _use_events = true;
static vk::BVH _tlas;
static lm::Array<vk::BVH> _blases;
static bool _recreate_swapchain = false;
static SceneConfig _saved_configs[INTEGRATOR_COUNT];
static bool _saved_config_valid[INTEGRATOR_COUNT] = {};
static i32 _current_integrator_idx = 0;

static const char* _integrator_display_names[INTEGRATOR_COUNT] = {
	"Path", "BDPT", "SPPM", "VCM", "PSSMLT", "SMLT", "VCMMLT", "ReSTIR", "ReSTIR GI", "DDGI", "ReSTIR PT", "IR Cache",
};

static const lm::String _integrator_config_names[INTEGRATOR_COUNT] = {
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
	rg::request_shader_reload();
	vk::shader_arena_reset();
}

static void cache_active_config() {
	SceneConfig& config = scene::get()->config;
	_saved_configs[config.type] = config;
	_saved_config_valid[config.type] = true;
}

static void select_integrator_config(IntegratorType type, const SceneCommon& common) {
	if (_saved_config_valid[type]) {
		scene::get()->config = _saved_configs[type];
		scene::get()->config.common = common;
	} else {
		scene::config_init(_integrator_config_names[type], common);
	}
	scene::get()->config.type = type;
	scene::get()->config.common.integrator_name = _integrator_config_names[type];
}

static void key_callback(void*, KeyInput, KeyAction) {
	if (Window::is_key_down(KeyInput::KEY_F1)) {
		_show_ui = !_show_ui;
	}
	if (Window::is_key_down(KeyInput::KEY_F10)) {
		_write_exr = true;
	} else if (Window::is_key_down(KeyInput::KEY_F11)) {
		_comparison_mode ^= true;
	} else if (Window::is_key_down(KeyInput::KEY_F5)) {
		reload_shaders();
		_active_integrator.updated = true;
	} else if (Window::is_key_down(KeyInput::KEY_F6)) {
		_capture_ref_img = true;
	} else if (Window::is_key_down(KeyInput::KEY_F7)) {
		_capture_target_img = true;
	} else if (_comparison_mode && Window::is_key_down(KeyInput::KEY_LEFT)) {
		_comparison_img_toggle = false;
	} else if (_comparison_mode && Window::is_key_down(KeyInput::KEY_RIGHT)) {
		_comparison_img_toggle = true;
	}
}

void init(bool use_debug, i32 argc, char* argv[]) {
	_debug = use_debug;
	lm::String scene_name = CSTR("scenes/caustics.scene");
	for (i32 i = 0; i < argc; i++) {
		// +1 for null terminator
		lm::String arg_str = lm::String(argv[i], strlen(argv[i]) + 1);
		if (lm::str_ends_with(arg_str, CSTR(".scene"))) {
			scene_name = arg_str;
		}
	}
	srand((u32)(os::time_seconds() * 1000000000.0));
	Window::add_key_callback(key_callback);

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

	vk::init(_debug);
	imgui_renderer::init(&_imgui_renderer);
	_initialized = true;

	// Enable shader reflections for the render graph
	rg::settings().shader_inference = _enable_shader_inference;
	// Event based synchronization instead of barriers
	rg::settings().use_events = _use_events;

	scene::load(scene_name);
	_current_integrator_idx = i32(scene::get()->config.type);
	cache_active_config();
	_active_integrator.tlas = &_tlas;
	integrator::set_type(&_active_integrator, scene::get()->config.type);
	integrator::init(&_active_integrator);
	if (!_tlas.accel) {
		integrator::create_accel(&_active_integrator, &_tlas, &_blases);
	}
	post_fx::init(&_post_fx);
	init_resources();
}

static void init_resources() {
	u32 viewport_size = Window::width() * Window::height();
	_output_img_buffer =
		prm::get_buffer({.name = CSTR("Output Image Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = viewport_size * 4 * 4});

	_output_img_buffer_cpu =
		prm::get_buffer({.name = CSTR("Output Image CPU"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
						 .size = viewport_size * 4 * 4});

	_residual_buffer =
		prm::get_buffer({.name = CSTR("RMSE Residual"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = viewport_size * 4});

	_counter_buffer =
		prm::get_buffer({.name = CSTR("RMSE Counter"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(i32)});

	_rmse_val_buffer =
		prm::get_buffer({.name = CSTR("RMSE Value"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
						 .size = sizeof(f32)});
	vk::TextureDesc texture_desc = {.name = CSTR("Reference Texture"),
									.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
											 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
									.dimensions = {Window::width(), Window::height(), 1},
									.format = VK_FORMAT_R32G32B32A32_SFLOAT,
									.initial_layout = VK_IMAGE_LAYOUT_GENERAL};
	_reference_tex = prm::get_texture(texture_desc);
	texture_desc.name = CSTR("Target Texture");
	_target_tex = prm::get_texture(texture_desc);

	RTUtilsDesc rt_utils_desc;
	if (_load_reference) {
		// Load the ground truth image
		i32 width, height;
		f32* data = ImageUtils::load_exr("out.exr", width, height);
		if (!data) {
			LUMEN_ERROR("Could not load the reference image");
		}
		_gt_img_buffer =
			prm::get_buffer({.name = CSTR("Ground Truth Image"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = Window::width() * Window::height() * 4 * sizeof(f32),
							 .data = data});
		rt_utils_desc.gt_img_addr = _gt_img_buffer->device_address();
		free(data);
	}

	rt_utils_desc.out_img_addr = _output_img_buffer->device_address();
	rt_utils_desc.residual_addr = _residual_buffer->device_address();
	rt_utils_desc.counter_addr = _counter_buffer->device_address();
	rt_utils_desc.rmse_val_addr = _rmse_val_buffer->device_address();

	_rt_utils_desc_buffer =
		prm::get_buffer({.name = CSTR("RT Utils Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(RTUtilsDesc),
						 .data = &rt_utils_desc});

	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, out_img_addr, _output_img_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, residual_addr, _residual_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, counter_addr, _counter_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, rmse_val_addr, _rmse_val_buffer);
}

static void cleanup_resources() {
	vk::Buffer** buffers[] = {&_output_img_buffer, &_output_img_buffer_cpu, &_residual_buffer, &_counter_buffer,
							  &_rmse_val_buffer,	  &_rt_utils_desc_buffer,  &_gt_img_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
	vk::Texture** textures[] = {&_reference_tex, &_target_tex};
	for (vk::Texture** texture : textures) {
		prm::remove(*texture);
		*texture = nullptr;
	}
}

void update() {
	f32 frame_time = draw_frame();
	_cpu_avg_time = (1.0f - 1.0f / (_cnt)) * _cpu_avg_time + frame_time / (f32)_cnt;
	_cpu_avg_time = 0.95f * _cpu_avg_time + 0.05f * frame_time;
	integrator::update(&_active_integrator);
	_active_integrator.updated = false;
#if 0
	char* stats = nullptr;
	vmaBuildStatsString(vk::context().allocator, &stats, VK_TRUE);
	printf("Stats--\n");
	LUMEN_TRACE("%s", stats);
#endif
}

static void render(u32 i) {
	integrator::render(&_active_integrator);
	vk::Texture* input_tex = nullptr;
	if (_comparison_mode && _img_captured) {
		input_tex = _comparison_img_toggle ? _target_tex : _reference_tex;
	} else {
		input_tex = _active_integrator.output_tex;
	}
	vk::Texture* output = vk::swapchain_images()[i];
	post_fx::add_passes(&_post_fx, input_tex, output);
	render_debug_utils();
	imgui_renderer::add_pass(&_imgui_renderer, output);

	VkCommandBuffer cmdbuf = vk::context().command_buffers[i];
	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vk::check(vkBeginCommandBuffer(cmdbuf, &begin_info));
	rg::run(cmdbuf);
	vk::check(vkEndCommandBuffer(cmdbuf));
}

static void render_debug_utils() {
	if (_write_exr) {
		rg::current_pass().copy(_active_integrator.output_tex, _output_img_buffer_cpu);
	} else if (_capture_ref_img) {
		rg::current_pass().copy(_active_integrator.output_tex, _reference_tex);

	} else if (_capture_target_img) {
		rg::current_pass().copy(_active_integrator.output_tex, _target_tex);
	}

	if (_capture_ref_img || _capture_target_img) {
		_img_captured = true;
		_capture_ref_img = false;
		_capture_target_img = false;
	}

	if (_calc_rmse && _has_gt) {
		auto op_reduce = [&](const lm::String& op_name, const lm::String& op_shader_name, const lm::String& reduce_name,
							 const lm::String& reduce_shader_name) {
			u32 num_wgs = u32((Window::width() * Window::height() + 1023) / 1024);
			rg::add_compute(op_name, {.shader = vk::Shader(op_shader_name), .dims = {num_wgs, 1, 1}})
				.push_constants(&_rt_utils_pc)
				.bind(_rt_utils_desc_buffer)
				.zero({_residual_buffer, _counter_buffer});
			while (num_wgs != 1) {
				rg::add_compute(reduce_name, {.shader = vk::Shader(reduce_shader_name), .dims = {num_wgs, 1, 1}})
					.push_constants(&_rt_utils_pc)
					.bind(_rt_utils_desc_buffer);
				num_wgs = (num_wgs + 1023) / 1024;
			}
		};
		rg::current_pass().copy(_active_integrator.output_tex, _output_img_buffer);
		// Calculate RMSE
		op_reduce(CSTR("OpReduce: RMSE"), CSTR("src/shaders/rmse/calc_rmse.comp"), CSTR("OpReduce: Reduce RMSE"),
				  CSTR("src/shaders/rmse/reduce_rmse.comp"));
		rg::add_compute(CSTR("Calculate RMSE"),
						  {.shader = vk::Shader(CSTR("src/shaders/rmse/output_rmse.comp")), .dims = {1, 1, 1}})
			.push_constants(&_rt_utils_pc)
			.bind(_rt_utils_desc_buffer);
	}
}

static bool gui() {
	util::Slice<GPUQueryManager::TimestampData> query_results = GPUQueryManager::get();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	ImGui::Text("General settings:");
	ImGui::PopStyleColor();
	ImGui::Text("Frame %d time (CPU) %.2f ms ( %.2f FPS )", _active_integrator.frame_num, _cpu_avg_time,
				1000 / _cpu_avg_time);
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
			ImGui::Text("%*s%.2f ms: %.*s", scope * 2, "", elapsed_ms, (int)data.name.size, data.name.data);
		}
	}

	ImGui::Text("GPU Memory Usage: %.2f MB", vk::get_memory_usage(vk::context().physical_device) / (1024.0f * 1024.0f));
	u64 arena_total_used_bytes = 0;
	u64 arena_total_allocated_bytes = 0;
	lm::get_all_arena_stats(arena_total_used_bytes, arena_total_allocated_bytes);
	ImGui::Text("CPU Arena Usage: %.2f MB Allocated, %.2f MB Used", arena_total_allocated_bytes / (1024.0f * 1024.0f),
				arena_total_used_bytes / (1024.0f * 1024.0f));
	bool updated = false;
	ImGui::Checkbox("Show camera statistics", &_show_cam_stats);
	if (_show_cam_stats) {
		const lm::Camera& camera = scene::get()->camera;
		ImGui::Text("X - Right, Y - Up, -Z - Forward");
		ImGui::Text("Camera position: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
		ImGui::Text("Camera rotation (degrees): X:%.2f Y:%.2f Z:%.2f", camera.rotation.x, camera.rotation.y,
					camera.rotation.z);
		ImGui::Text("Camera direction:  %.2f %.2f %.2f", camera.direction.x, camera.direction.y, camera.direction.z);
		if (ImGui::Button("Copy camera data to clipboard")) {
			char cam_pos_str[256];
			stbsp_snprintf(
				cam_pos_str, sizeof(cam_pos_str),
				"    \"position\": [%.2f,%.2f,%.2f],\n    \"rotation\":[%.2f,%.2f,%.2f],\n    \"dir\":[%.2f,%.2f,%.2f]",
				camera.position.x, camera.position.y, camera.position.z, camera.rotation.x, camera.rotation.y,
				camera.rotation.z, camera.direction.x, camera.direction.y, camera.direction.z);
			Window::set_clipboard_text(cam_pos_str);
		}
	}
	if (ImGui::Checkbox("Enable VSync", &vk::context().vsync_enabled)) {
		_recreate_swapchain = true;
	}
	if (ImGui::Button("Reload shaders (F5)")) {
		reload_shaders();
		updated |= true;
	}
	ImGui::Checkbox("Comparison mode (F11)", &_comparison_mode);
	if (_comparison_mode && _img_captured) {
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
		const char* texts[] = {"Showing: Reference Image", "Showing: Target Image"};
		ImGui::Text("%s\n", texts[u32(_comparison_img_toggle)]);
		ImGui::PopStyleColor();
	}
	if (ImGui::Button("Capture reference image (F6)")) {
		_capture_ref_img = true;
	}
	if (ImGui::Button("Capture target image (F7)")) {
		_capture_target_img = true;
	}

	SceneConfig& config = scene::get()->config;
	if (ImGui::BeginCombo("Select Integrator", _integrator_display_names[_current_integrator_idx])) {
		for (i32 n = 0; n < INTEGRATOR_COUNT; n++) {
			const bool selected = _current_integrator_idx == n;
			if (ImGui::Selectable(_integrator_display_names[n], selected)) {
				_current_integrator_idx = n;
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (_current_integrator_idx != i32(config.type)) {
		updated = true;
		vkDeviceWaitIdle(vk::context().device);
		const bool was_custom_accel = config.type == INTEGRATOR_DDGI;
		SceneCommon common = config.common;
		cache_active_config();
		integrator::destroy(&_active_integrator, /*resize=*/false);
		IntegratorType new_type = IntegratorType(_current_integrator_idx);
		select_integrator_config(new_type, common);
		GPUQueryManager::reset_data();
		integrator::set_type(&_active_integrator, new_type);
		const bool is_custom_accel = new_type == INTEGRATOR_DDGI;
		integrator::init(&_active_integrator);
		if (was_custom_accel || is_custom_accel) {
			destroy_accel();
			integrator::create_accel(&_active_integrator, &_tlas, &_blases);
		}
	}
	return updated;
}

static f32 draw_frame() {
	if (_cnt == 0) {
		_start = os::time_seconds();
	}

	f64 t_begin = os::time_seconds() * 1000;
	bool updated = false;
	u32 image_idx = vk::prepare_frame();
	if (image_idx == UINT32_MAX) {
		f64 t_end = os::time_seconds() * 1000;
		f64 t_diff = t_end - t_begin;
		return (f32)t_diff;
	}
	imgui_renderer::new_frame(&_imgui_renderer);

	_active_integrator.updated |= updated;
	if (_show_ui) {
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
		ImGui::Begin("Debug (F1 to hide)", &_show_ui);
		bool gui_updated = gui();
		gui_updated |= integrator::gui(&_active_integrator);
		gui_updated |= post_fx::gui(&_post_fx);
		static bool _show_imgui_demo = false;
		if (ImGui::Button("Show ImGui Demo")) {
			_show_imgui_demo = !_show_imgui_demo;
		}
		if (_show_imgui_demo) {
			ImGui::ShowDemoWindow(&_show_imgui_demo);
		}
		ImGui::End();
		_active_integrator.updated |= gui_updated;
	}

	render(image_idx);
	VkResult result = vk::submit_frame(image_idx);
	rg::reset_frame();
	if (result != VK_SUCCESS) {
		Window::update_window_size();
		cleanup_resources();
		integrator::destroy(&_active_integrator, /*resize=*/true);
		post_fx::destroy(&_post_fx);

		integrator::init(&_active_integrator);
		post_fx::init(&_post_fx);
		init_resources();
		_active_integrator.updated = true;
	}

	if (_recreate_swapchain) {
		vk::recreate_swap_chain();
		_recreate_swapchain = false;
	}

	f64 now = os::time_seconds();
	f64 diff = now - _start;

	if (_write_exr) {
		_write_exr = false;
		ImageUtils::save_exr((f32*)vk::buffer_map(_output_img_buffer_cpu), Window::width(), Window::height(), "out.exr");
		vk::buffer_unmap(_output_img_buffer_cpu);
	}
	bool time_limit = abs(diff - 5.0) < 0.1;
	_calc_rmse = time_limit;

	if (_calc_rmse && _has_gt) {
		f32 rmse = *(f32*)vk::buffer_map(_rmse_val_buffer);
		vk::buffer_unmap(_rmse_val_buffer);
		LUMEN_TRACE("RMSE: %f", rmse * 1e6);
		_start = now;
	}
	f64 t_end = os::time_seconds() * 1000;
	f64 t_diff = t_end - t_begin;
	_cnt++;
	return (f32)t_diff;
}

static void destroy_accel() {
	_tlas.destroy();
	for (vk::BVH& blas : _blases) {
		blas.destroy();
	}
	_blases.clear();
}

void cleanup() {
	vkDeviceWaitIdle(vk::context().device);
	if (_initialized) {
		cleanup_resources();
		integrator::destroy(&_active_integrator, /*resize=*/false);
		post_fx::destroy(&_post_fx);
		scene::destroy();
		destroy_accel();
		imgui_renderer::destroy(&_imgui_renderer);
		vk::cleanup();
		_initialized = false;
	}
}

}  // namespace ray_tracer
