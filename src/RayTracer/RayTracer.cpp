#include "Framework/RenderGraph.h"
#include "LumenPCH.h"
#include <tinyexr.h>
#define TINYGLTF_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "RayTracer.h"

using namespace lumen;

RayTracer* RayTracer::instance = nullptr;
bool load_reference = false;
bool calc_rmse = false;

RayTracer::RayTracer(bool debug, int argc, char* argv[]) : debug(debug) {
	instance = this;
	parse_args(argc, argv);
}

void RayTracer::init() {
	srand((uint32_t)time(NULL));
	Window::add_key_callback([this](KeyInput key, KeyAction action) {
		if (Window::is_key_down(KeyInput::KEY_F1)) {
			show_ui = !show_ui;
		}
		if (Window::is_key_down(KeyInput::KEY_F10)) {
			write_exr = true;
		} else if (Window::is_key_down(KeyInput::KEY_F11)) {
			comparison_mode ^= true;
		} else if (Window::is_key_down(KeyInput::KEY_F5)) {
			vk::render_graph()->reload_shaders = true;
			vk::render_graph()->shader_cache.clear();
			integrator->updated = true;
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

	vk::init(debug);
	initialized = true;

	// Enable shader reflections for the render graph
	vk::render_graph()->settings.shader_inference = enable_shader_inference;
	// Event based synchronization instead of barriers
	vk::render_graph()->settings.use_events = use_events;

	scene.load_scene(scene_name);
	create_integrator(int(scene.config->integrator_type));
	integrator->init();
	if (!tlas.accel) {
		integrator->create_accel(tlas, blases);
	}
	post_fx.init();
	init_resources();
	LUMEN_TRACE("Memory usage {} MB", vk::get_memory_usage(vk::context().physical_device) * 1e-6);
}

void RayTracer::init_resources() {
	uint32_t viewport_size = Window::width() * Window::height();
	output_img_buffer =
		prm::get_buffer({.name = "Output Image Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = viewport_size * 4 * 4});

	output_img_buffer_cpu =
		prm::get_buffer({.name = "Output Image CPU",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU_TO_CPU,
						 .size = viewport_size * 4 * 4});

	residual_buffer =
		prm::get_buffer({.name = "RMSE Residual",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = viewport_size * 4});

	counter_buffer =
		prm::get_buffer({.name = "RMSE Counter",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(int)});

	rmse_val_buffer =
		prm::get_buffer({.name = "RMSE Value",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU_TO_CPU,
						 .size = sizeof(float)});
	auto texture_desc = vk::TextureDesc{.name = "Reference Texture",
										.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
												 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
										.dimensions = {Window::width(), Window::height(), 1},
										.format = VK_FORMAT_R32G32B32A32_SFLOAT,
										.initial_layout = VK_IMAGE_LAYOUT_GENERAL};
	reference_tex = prm::get_texture(texture_desc);
	texture_desc.name = "Target Texture";
	target_tex = prm::get_texture(texture_desc);

	RTUtilsDesc rt_utils_desc;
	if (load_reference) {
		// Load the ground truth image
		int width, height;
		float* data = ImageUtils::load_exr("out.exr", width, height);
		if (!data) {
			LUMEN_ERROR("Could not load the reference image");
		}
		gt_img_buffer =
			prm::get_buffer({.name = "Ground Truth Image",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BufferType::GPU,
							 .size = Window::width() * Window::height() * 4 * sizeof(float),
							 .data = data});
		rt_utils_desc.gt_img_addr = gt_img_buffer->get_device_address();
		free(data);
	}

	rt_utils_desc.out_img_addr = output_img_buffer->get_device_address();
	rt_utils_desc.residual_addr = residual_buffer->get_device_address();
	rt_utils_desc.counter_addr = counter_buffer->get_device_address();
	rt_utils_desc.rmse_val_addr = rmse_val_buffer->get_device_address();

	rt_utils_desc_buffer =
		prm::get_buffer({.name = "RT Utils Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(RTUtilsDesc),
						 .data = &rt_utils_desc});

	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, out_img_addr, output_img_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, residual_addr, residual_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, counter_addr, counter_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, rmse_val_addr, rmse_val_buffer, vk::render_graph());
}

void RayTracer::cleanup_resources() {
	std::vector<vk::Buffer*> buffer_list = {output_img_buffer, output_img_buffer_cpu, residual_buffer,
											counter_buffer,	   rmse_val_buffer,		  rt_utils_desc_buffer};
	std::vector<vk::Texture*> tex_list = {reference_tex, target_tex};
	if (load_reference) {
		buffer_list.push_back(gt_img_buffer);
	}
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
	for (auto t : tex_list) {
		prm::remove(t);
	}
}

void RayTracer::update() {
	float frame_time = draw_frame();
	cpu_avg_time = (1.0f - 1.0f / (cnt)) * cpu_avg_time + frame_time / (float)cnt;
	cpu_avg_time = 0.95f * cpu_avg_time + 0.05f * frame_time;
	integrator->update();
	integrator->updated = false;
#if 0
	char* stats = nullptr;
	vmaBuildStatsString(vk::context().allocator, &stats, VK_TRUE);
	printf("Stats--\n");
	LUMEN_TRACE("{}", stats);
#endif
}

void RayTracer::render(uint32_t i) {
	integrator->render();
	vk::Texture* input_tex = nullptr;
	if (comparison_mode && img_captured) {
		input_tex = comparison_img_toggle ? target_tex : reference_tex;
	} else {
		input_tex = integrator->output_tex;
	}
	post_fx.render(input_tex, vk::swapchain_images()[i]);
	render_debug_utils();

	auto cmdbuf = vk::context().command_buffers[i];
	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vk::check(vkBeginCommandBuffer(cmdbuf, &begin_info));
	vk::render_graph()->run(cmdbuf);
	vk::check(vkEndCommandBuffer(cmdbuf));
}

void RayTracer::render_debug_utils() {
	if (write_exr) {
		vk::render_graph()->current_pass().copy(integrator->output_tex, output_img_buffer_cpu);
	} else if (capture_ref_img) {
		vk::render_graph()->current_pass().copy(integrator->output_tex, reference_tex);

	} else if (capture_target_img) {
		vk::render_graph()->current_pass().copy(integrator->output_tex, target_tex);
	}

	if (capture_ref_img || capture_target_img) {
		img_captured = true;
		capture_ref_img = false;
		capture_target_img = false;
	}

	if (calc_rmse && has_gt) {
		auto op_reduce = [&](const std::string& op_name, const std::string& op_shader_name,
							 const std::string& reduce_name, const std::string& reduce_shader_name) {
			uint32_t num_wgs = uint32_t((Window::width() * Window::height() + 1023) / 1024);
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
		vk::render_graph()->current_pass().copy(integrator->output_tex, output_img_buffer);
		// Calculate RMSE
		op_reduce("OpReduce: RMSE", "src/shaders/rmse/calc_rmse.comp", "OpReduce: Reduce RMSE",
				  "src/shaders/rmse/reduce_rmse.comp");
		vk::render_graph()
			->add_compute("Calculate RMSE",
						  {.shader = vk::Shader("src/shaders/rmse/output_rmse.comp"), .dims = {1, 1, 1}})
			.push_constants(&rt_utils_pc)
			.bind(rt_utils_desc_buffer);
	}
}

void RayTracer::create_integrator(int integrator_idx) {
	switch (integrator_idx) {
		case int(IntegratorType::Path):
			integrator = std::make_unique<Path>(&scene, tlas);
			break;
		case int(IntegratorType::BDPT):
			integrator = std::make_unique<BDPT>(&scene, tlas);
			break;
		case int(IntegratorType::SPPM):
			integrator = std::make_unique<SPPM>(&scene, tlas);
			break;
		case int(IntegratorType::VCM):
			integrator = std::make_unique<VCM>(&scene, tlas);
			break;
		case int(IntegratorType::ReSTIR):
			integrator = std::make_unique<ReSTIR>(&scene, tlas);
			break;
		case int(IntegratorType::ReSTIRGI):
			integrator = std::make_unique<ReSTIRGI>(&scene, tlas);
			break;
		case int(IntegratorType::ReSTIRPT):
			integrator = std::make_unique<ReSTIRPT>(&scene, tlas);
			break;
		case int(IntegratorType::PSSMLT):
			integrator = std::make_unique<PSSMLT>(&scene, tlas);
			break;
		case int(IntegratorType::SMLT):
			integrator = std::make_unique<SMLT>(&scene, tlas);
			break;
		case int(IntegratorType::VCMMLT):
			integrator = std::make_unique<VCMMLT>(&scene, tlas);
			break;
		case int(IntegratorType::DDGI):
			integrator = std::make_unique<DDGI>(&scene, tlas);
			break;
		default:
			break;
	}
}

bool RayTracer::gui() {
	util::Slice<GPUQueryManager::TimestampData> query_results = GPUQueryManager::get();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	ImGui::Text("General settings:");
	ImGui::PopStyleColor();
	ImGui::Text("Frame %d time (CPU) %.2f ms ( %.2f FPS )", integrator->frame_num, cpu_avg_time, 1000 / cpu_avg_time);
	double frame_time_gpu_ms = (GPUQueryManager::get_total_elapsed()) * 1e-6;
	if (frame_time_gpu_ms > 0) {
		ImGui::Text("Frame time (GPU) %.2f ms", frame_time_gpu_ms);
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
		ImGui::Text("Individual GPU timings:");
		ImGui::PopStyleColor();
		for (size_t i = 0; i < query_results.size; i++) {
			const GPUQueryManager::TimestampData& data = query_results[i];
			GPUQueryManager::TimestampData* parent = data.parent;
			bool is_root = parent == nullptr;
			uint32_t scope = 0;
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

	ImGui::Text("Memory Usage: %.2f MB", vk::get_memory_usage(vk::context().physical_device) * 1e-6);
	bool updated = false;
	ImGui::Checkbox("Show camera statistics", &show_cam_stats);
	if (show_cam_stats) {
		ImGui::PushItemWidth(170);
		ImGui::DragFloat4("", glm::value_ptr(scene.camera->camera[0]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(scene.camera->camera[1]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(scene.camera->camera[2]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(scene.camera->camera[3]), 0.05f);
	}
	if (ImGui::Button("Reload shaders (F5)")) {
		vk::render_graph()->reload_shaders = true;
		vk::render_graph()->shader_cache.clear();
		updated |= true;
	}
	ImGui::Checkbox("Comparison mode (F11)", &comparison_mode);
	if (comparison_mode && img_captured) {
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
		const char* texts[] = {"Showing: Reference Image", "Showing: Target Image"};
		ImGui::Text("%s\n", texts[uint32_t(comparison_img_toggle)]);
		ImGui::PopStyleColor();
	}
	if (ImGui::Button("Capture reference image (F6)")) {
		capture_ref_img = true;
	}
	if (ImGui::Button("Capture target image (F7)")) {
		capture_target_img = true;
	}

	const char* settings[] = {"Path",	"BDPT",	  "SPPM",	   "VCM",		"PSSMLT", "SMLT",
							  "VCMMLT", "ReSTIR", "ReSTIR GI", "ReSTIR PT", "DDGI"};

	static int curr_integrator_idx = int(scene.config->integrator_type);
	if (ImGui::BeginCombo("Select Integrator", settings[curr_integrator_idx])) {
		for (int n = 0; n < IM_ARRAYSIZE(settings); n++) {
			const bool selected = curr_integrator_idx == n;
			if (ImGui::Selectable(settings[n], selected)) {
				curr_integrator_idx = n;
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (curr_integrator_idx != int(scene.config->integrator_type)) {
		updated = true;
		vkDeviceWaitIdle(vk::context().device);
		bool was_custom_accel = typeid(*integrator) == typeid(DDGI);
		integrator->destroy(/*resize=*/false);
		RenderGraph* rg = vk::render_graph();
		REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, out_img_addr, output_img_buffer, rg);
		REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, residual_addr, residual_buffer, rg);
		REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, counter_addr, counter_buffer, rg);
		REGISTER_BUFFER_WITH_ADDRESS(RTUtilsDesc, desc, rmse_val_addr, rmse_val_buffer, rg);
		SceneConfig prev_scene_config = *scene.config;
		auto integrator_str = std::string(settings[curr_integrator_idx]);
		integrator_str.erase(std::remove_if(integrator_str.begin(), integrator_str.end(), ::isspace),
							 integrator_str.end());
		std::transform(integrator_str.begin(), integrator_str.end(), integrator_str.begin(), ::tolower);
		scene.create_scene_config(integrator_str);
		scene.config->cam_settings = prev_scene_config.cam_settings;
		scene.config->sky_col = prev_scene_config.sky_col;
		scene.config->path_length = prev_scene_config.path_length;
		GPUQueryManager::reset_data();
		create_integrator(curr_integrator_idx);
		bool is_custom_accel = typeid(*integrator) == typeid(DDGI);
		integrator->init();
		if (was_custom_accel || is_custom_accel) {
			destroy_accel();
			integrator->create_accel(tlas, blases);
		}
	}
	return updated;
}

float RayTracer::draw_frame() {
	if (cnt == 0) {
		start = clock();
	}

	auto t_begin = glfwGetTime() * 1000;
	bool updated = false;
	uint32_t image_idx = vk::prepare_frame();
	if (image_idx == UINT32_MAX) {
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		return (float)t_diff;
	}
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	integrator->updated |= updated;
	if (show_ui) {
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
		ImGui::Begin("Debug (F1 to hide)", &show_ui);
		bool gui_updated = gui();
		gui_updated |= integrator->gui();
		gui_updated |= post_fx.gui();
		static bool show_imgui_demo = false;
		if (ImGui::Button("Show ImGui Demo")) {
			show_imgui_demo = !show_imgui_demo;
		}
		if (show_imgui_demo) {
			ImGui::ShowDemoWindow(&show_imgui_demo);
		}
		ImGui::End();
		integrator->updated |= gui_updated;
	}

	render(image_idx);
	VkResult result = vk::submit_frame(image_idx);
	vk::render_graph()->reset();
	if (result != VK_SUCCESS) {
		Window::update_window_size();
		const float aspect_ratio = (float)Window::width() / Window::height();
		const PerspectiveCamera* old_cam = (PerspectiveCamera*)scene.camera.get();
		glm::vec3 old_rotation = old_cam->rotation;
		scene.camera = std::unique_ptr<lumen::PerspectiveCamera>(new lumen::PerspectiveCamera(
			old_cam->fov, 0.01f, 1000.0f, aspect_ratio, old_cam->direction, old_cam->position));
		scene.camera->rotation = old_rotation;
		cleanup_resources();
		integrator->destroy(/*resize=*/true);
		post_fx.destroy();
		vk::destroy_imgui();

		integrator->init();
		post_fx.init();
		init_resources();
		vk::init_imgui();
		integrator->updated = true;
	}

	auto now = clock();
	auto diff = ((float)now - start);

	if (write_exr) {
		write_exr = false;
		ImageUtils::save_exr((float*)vk::map_buffer(output_img_buffer_cpu), Window::width(), Window::height(),
							 "out.exr");
		vk::unmap_buffer(output_img_buffer_cpu);
	}
	bool time_limit = (abs(diff / CLOCKS_PER_SEC - 5)) < 0.1;
	calc_rmse = time_limit;

	if (calc_rmse && has_gt) {
		float rmse = *(float*)vk::map_buffer(rmse_val_buffer);
		vk::unmap_buffer(rmse_val_buffer);
		LUMEN_TRACE("RMSE {}", rmse * 1e6);
		start = now;
	}
	auto t_end = glfwGetTime() * 1000;
	auto t_diff = t_end - t_begin;
	cnt++;
	return (float)t_diff;
}

void RayTracer::parse_args(int argc, char* argv[]) {
	scene_name = "scenes/caustics.json";
	std::regex fn("(.*).(.json|.xml)");
	for (int i = 0; i < argc; i++) {
		if (std::regex_match(argv[i], fn)) {
			scene_name = argv[i];
		}
	}
}
void RayTracer::destroy_accel() {
	tlas.destroy();
	for (vk::BVH& blas : blases) {
		blas.destroy();
	}
	blases.clear();
}

void RayTracer::cleanup() {
	vkDeviceWaitIdle(vk::context().device);
	if (initialized) {
		cleanup_resources();
		integrator->destroy(/*resize=*/false);
		post_fx.destroy();
		scene.destroy();
		destroy_accel();
		vk::destroy_imgui();
		vk::cleanup();
	}
}
