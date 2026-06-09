#include "Integrator.h"
#include "VCM.h"
const i32 max_samples = 50000;
namespace vcm {

void init(Integrator* integrator) {
	VCM& state = integrator->vcm;


	state.photon_buffer =
		prm::get_buffer({.name = CSTR("Photon Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = 10 * Window::width() * Window::height() * sizeof(VCMPhotonHash)});

	state.vcm_light_vertices_buffer =
		prm::get_buffer({.name = CSTR("VCM Light Vertices"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * (integrator->lumen_scene->config.common.path_length + 1) *
								 sizeof(VCMVertex)});

	state.light_path_cnt_buffer =
		prm::get_buffer({.name = CSTR("Light Path Count"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(f32)});

	state.color_storage_buffer =
		prm::get_buffer({.name = CSTR("Color Storage"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * 3 * sizeof(f32)});

	state.vcm_reservoir_buffer =
		prm::get_buffer({.name = CSTR("VCM Reservoirs"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(VCMReservoir)});

	state.light_samples_buffer =
		prm::get_buffer({.name = CSTR("Light Samples"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(VCMRestirData)});

	state.should_resample_buffer =
		prm::get_buffer({.name = CSTR("Should Resample"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = 4});

	state.light_state_buffer =
		prm::get_buffer({.name = CSTR("Light States"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(LightState)});

	state.angle_struct_buffer =
		prm::get_buffer({.name = CSTR("Angle Struct"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = max_samples * sizeof(AngleStruct)});

	state.avg_buffer = prm::get_buffer({.name = CSTR("Average"),
								  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
										   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								  .memory_type = vk::BUFFER_TYPE_GPU,
								  .size = sizeof(AvgStruct)});

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// VCM
	desc.photon_addr = state.photon_buffer->device_address();
	desc.vcm_vertices_addr = state.vcm_light_vertices_buffer->device_address();
	desc.path_cnt_addr = state.light_path_cnt_buffer->device_address();
	desc.color_storage_addr = state.color_storage_buffer->device_address();

	desc.vcm_reservoir_addr = state.vcm_reservoir_buffer->device_address();
	desc.light_samples_addr = state.light_samples_buffer->device_address();
	desc.should_resample_addr = state.should_resample_buffer->device_address();
	desc.light_state_addr = state.light_state_buffer->device_address();
	desc.angle_struct_addr = state.angle_struct_buffer->device_address();
	desc.avg_addr = state.avg_buffer->device_address();

	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});
	state.pc.total_light_area = 0;

	integrator->frame_num = 0;

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, state.photon_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, state.vcm_light_vertices_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, state.light_path_cnt_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, state.color_storage_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_reservoir_addr, state.vcm_reservoir_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_samples_addr, state.light_samples_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, should_resample_addr, state.should_resample_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_state_addr, state.light_state_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, angle_struct_addr, state.angle_struct_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, avg_addr, state.avg_buffer, vk::render_graph());
}

void render(Integrator* integrator) {
	VCM& state = integrator->vcm;
	const VCMConfig& config = integrator->lumen_scene->config.settings.vcm;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = i32(integrator->lumen_scene->gpu_lights.size);
	state.pc.time = rand() % UINT_MAX;
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.frame_num = integrator->frame_num;
	// VCM related constants
	state.pc.radius = integrator->lumen_scene->dimensions.radius * config.radius_factor / 100.f;
	state.pc.radius /= (f32)pow((double)state.pc.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	state.pc.min_bounds = integrator->lumen_scene->dimensions.min;
	state.pc.max_bounds = integrator->lumen_scene->dimensions.max;
	state.pc.use_vm = config.enable_vm;
	state.pc.use_vc = state.use_vc;
	state.pc.do_spatiotemporal = state.do_spatiotemporal;
	state.pc.random_num = rand() % UINT_MAX;
	state.pc.max_angle_samples = max_samples;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};
	const glm::vec3 diam = state.pc.max_bounds - state.pc.min_bounds;
	const f32 max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const i32 base_grid_res = i32(max_comp / state.pc.radius);
	state.pc.grid_res = glm::max(ivec3(diam * f32(base_grid_res) / max_comp), ivec3(1));
	// Prepare
	auto& prepare_pass =
		vk::render_graph()
			->add_compute(CSTR("Init Reservoirs"),
						  {.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/init_reservoirs.comp")),
						   .dims = {(u32)std::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
			.push_constants(&state.pc)
			.bind(integrator->lumen_scene->scene_desc_buffer)
			.zero(state.photon_buffer, config.enable_vm);

	if (!state.do_spatiotemporal) {
		prepare_pass.zero({state.light_samples_buffer, state.should_resample_buffer});
	} else {
		prepare_pass.skip_execution();
	}

	// Do resampling
	vk::render_graph()
		->add_rt(CSTR("Resample"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/vcm/vcm_sample.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	// Check resampling
	vk::render_graph()
		->add_compute(CSTR("Check Reservoirs"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/check_reservoirs.comp")),
					   .dims = {(u32)std::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer)
		.zero(state.should_resample_buffer);
	state.pc.random_num = rand() % UINT_MAX;
	// Spawn light rays
	vk::render_graph()
		->add_rt(CSTR("VCM - Spawn Light"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/vcm/vcm_spawn_light.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.zero(state.light_state_buffer)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	state.pc.random_num = rand() % UINT_MAX;
	// Trace spawned rays
	vk::render_graph()
		->add_rt(CSTR("VCM - Trace Light"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/vcm/vcm_light.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Select a reservoir sample
	vk::render_graph()
		->add_compute(CSTR("Select Reservoir"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/select_reservoirs.comp")),
					   .dims = {(u32)std::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.bind(integrator->lumen_scene->scene_desc_buffer)
		.push_constants(&state.pc);

	// Update temporal reservoirs with the selected sample
	vk::render_graph()
		->add_compute(CSTR("Update Reservoirs"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/update_reservoirs.comp")),
					   .dims = {(u32)std::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.bind(integrator->lumen_scene->scene_desc_buffer)
		.push_constants(&state.pc);
	// Trace rays from eye
	vk::render_graph()
		->add_rt(CSTR("VCM - Trace Eye"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/vcm/vcm_eye.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	if (!state.do_spatiotemporal) {
		state.do_spatiotemporal = true;
	}
	state.pc.total_frame_num++;
}

bool update(Integrator* integrator) {
	VCM& state = integrator->vcm;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}
void destroy(Integrator* integrator, bool resize) {
	VCM& state = integrator->vcm;
	(void)resize;

	vk::Buffer** buffers[] = {&state.photon_buffer,
							 &state.vcm_light_vertices_buffer,
							 &state.light_path_cnt_buffer,
							 &state.color_storage_buffer,
							 &state.vcm_reservoir_buffer,
							 &state.light_samples_buffer,
							 &state.light_state_buffer,
							 &state.should_resample_buffer,
							 &state.angle_struct_buffer,
							 &state.avg_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
	if (state.desc_set_layout) vkDestroyDescriptorSetLayout(vk::context().device, state.desc_set_layout, nullptr);
	if (state.desc_pool) vkDestroyDescriptorPool(vk::context().device, state.desc_pool, nullptr);
	state.desc_set_layout = VK_NULL_HANDLE;
	state.desc_pool = VK_NULL_HANDLE;
}

bool gui(Integrator* integrator) {
	VCM& state = integrator->vcm;
	VCMConfig& config = integrator->lumen_scene->config.settings.vcm;
	bool result = false;
	bool path_length_changed = ImGui::SliderInt("Path length", (i32*)&integrator->lumen_scene->config.common.path_length, 0, 12);
	result |= path_length_changed;
	result |= ImGui::Checkbox("Enable VM", &config.enable_vm);
	return result;
}

}  // namespace vcm
