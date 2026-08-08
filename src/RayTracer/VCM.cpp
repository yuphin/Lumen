#include "Integrator.h"
#include "VCM.h"
constexpr i32 MAX_SAMPLES = 50000;
namespace vcm {

void init(Integrator* integrator) {
	VCM& state = integrator->vcm;
	state.do_spatiotemporal = false;
	state.pc.do_spatiotemporal = 0;
	state.pc.total_frame_num = 0;
	state.radius_factor = integrator->lumen_scene->config.settings.vcm.radius_factor;

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
						 .size = Window::width() * Window::height() *
								 (integrator->lumen_scene->config.common.path_length + 1) * sizeof(VCMVertex)});

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

	if (state.enable_ray_guiding) {
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
							 .size = MAX_SAMPLES * sizeof(AngleStruct)});

		state.avg_buffer =
			prm::get_buffer({.name = CSTR("Average"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
									  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = sizeof(AvgStruct)});
	}

	SceneDesc desc = integrator::scene_desc_base(integrator);
	// VCM
	SET_SCENE_BUFFER(desc, photon, state.photon_buffer);
	SET_SCENE_BUFFER(desc, vcm_vertices, state.vcm_light_vertices_buffer);
	SET_SCENE_BUFFER(desc, path_cnt, state.light_path_cnt_buffer);
	SET_SCENE_BUFFER(desc, color_storage, state.color_storage_buffer);

	if (state.enable_ray_guiding) {
		SET_SCENE_BUFFER(desc, vcm_reservoir, state.vcm_reservoir_buffer);
		SET_SCENE_BUFFER(desc, light_samples, state.light_samples_buffer);
		SET_SCENE_BUFFER(desc, should_resample, state.should_resample_buffer);
		SET_SCENE_BUFFER(desc, light_state, state.light_state_buffer);
		SET_SCENE_BUFFER(desc, angle_struct, state.angle_struct_buffer);
		SET_SCENE_BUFFER(desc, avg, state.avg_buffer);
	}
	integrator::upload_scene_desc(integrator, desc);
	integrator->frame_num = 0;

	assert(rg::settings().shader_inference == true);
}

void render(Integrator* integrator) {
	VCM& state = integrator->vcm;
	const VCMConfig& config = integrator->lumen_scene->config.settings.vcm;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = i32(integrator->lumen_scene->gpu_lights.size);
	state.pc.time = lm::rand_u32();
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.frame_num = integrator->frame_num;
	state.pc.enable_accumulation = state.enable_accumulation;
	// VCM related constants
	if(config.enable_vm) {
		state.pc.radius = integrator->lumen_scene->dimensions.radius * state.radius_factor / 100.f;
		state.pc.radius /= (f32)pow((double)state.pc.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	}
	state.pc.min_bounds = integrator->lumen_scene->dimensions.min;
	state.pc.max_bounds = integrator->lumen_scene->dimensions.max;
	state.pc.use_vm = config.enable_vm;
	state.pc.use_vc = state.use_vc;
	if (state.enable_ray_guiding) {
		state.pc.do_spatiotemporal = state.do_spatiotemporal;
		state.pc.max_angle_samples = MAX_SAMPLES;
	}
	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};
	const lm::vec3 diam = state.pc.max_bounds - state.pc.min_bounds;
	const f32 max_comp = lm::max(diam.x, lm::max(diam.y, diam.z));
	const i32 base_grid_res = i32(max_comp / state.pc.radius);
	state.pc.grid_res = lm::max(ivec3(diam * f32(base_grid_res) / max_comp), ivec3(1));
	if (state.enable_ray_guiding) {
		lm::RenderPass& prepare_pass =
			rg::add_compute(CSTR("Init Reservoirs"),
							{.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/init_reservoirs.comp")),
							 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
				.push_constants(&state.pc)
				.bind(integrator->lumen_scene->scene_desc_buffer)
				.zero(state.photon_buffer, config.enable_vm);

		if (!state.do_spatiotemporal) {
			prepare_pass.zero({state.light_samples_buffer, state.should_resample_buffer});
		} else {
			prepare_pass.skip_execution();
		}

		state.pc.random_num = lm::rand_u32();
		rg::add_rt(CSTR("Resample"),
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

		rg::add_compute(CSTR("Check Reservoirs"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/check_reservoirs.comp")),
						 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
			.push_constants(&state.pc)
			.bind(integrator->lumen_scene->scene_desc_buffer)
			.zero(state.should_resample_buffer);

		state.pc.random_num = lm::rand_u32();
		rg::add_rt(CSTR("VCM - Spawn Light"),
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
	}

	state.pc.random_num = lm::rand_u32();
	lm::SpecializationConstantArray light_specialization;
	if (state.enable_ray_guiding) {
		light_specialization = {1};
	}
	lm::RenderPass& light_pass = rg::add_rt(CSTR("VCM - Trace Light"),
											{
												.shaders = {{CSTR("src/shaders/integrators/vcm/vcm_light.rgen")},
															{CSTR("src/shaders/ray.rmiss")},
															{CSTR("src/shaders/ray_shadow.rmiss")},
															{CSTR("src/shaders/ray.rchit")},
															{CSTR("src/shaders/ray.rahit")}},
												.specialization_data = light_specialization,
												.dims = {Window::width(), Window::height()},
											})
									 .push_constants(&state.pc)
									 .bind(rt_bindings)
									 .bind(integrator->lumen_scene->mesh_lights_buffer)
									 .bind_texture_array(integrator->lumen_scene->scene_textures)
									 .bind_tlas(*integrator->tlas);
	if (!state.enable_ray_guiding) {
		light_pass.zero(state.photon_buffer, config.enable_vm);
	}

	if (state.enable_ray_guiding) {
		rg::add_compute(CSTR("Select Reservoir"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/select_reservoirs.comp")),
						 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
			.bind(integrator->lumen_scene->scene_desc_buffer)
			.push_constants(&state.pc);

		rg::add_compute(CSTR("Update Reservoirs"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/vcm/update_reservoirs.comp")),
						 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
			.bind(integrator->lumen_scene->scene_desc_buffer)
			.push_constants(&state.pc);
	}
	// Trace rays from eye
	rg::add_rt(CSTR("VCM - Trace Eye"),
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

	if (state.enable_ray_guiding) {
		if (!state.do_spatiotemporal) {
			state.do_spatiotemporal = true;
		}
		state.pc.total_frame_num++;
	}
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

	vk::Buffer** buffers[] = {&state.photon_buffer,			&state.vcm_light_vertices_buffer,
							  &state.light_path_cnt_buffer, &state.color_storage_buffer,
							  &state.vcm_reservoir_buffer,	&state.light_samples_buffer,
							  &state.light_state_buffer,	&state.should_resample_buffer,
							  &state.angle_struct_buffer,	&state.avg_buffer};
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
	bool path_length_changed =
		ImGui::SliderInt("Path length", (i32*)&integrator->lumen_scene->config.common.path_length, 0, 12);
	result |= path_length_changed;
	result |= ImGui::Checkbox("Enable VM", &config.enable_vm);

	ImGui::BeginDisabled(!config.enable_vm);
	ImGui::Text("Scene radius (base): %f\n", integrator->lumen_scene->dimensions.radius);
	ImGui::Text("Current radius: %f\n", state.pc.radius);
	result |= ImGui::SliderFloat("Radius factor (%)", &state.radius_factor, 0.0f, 100.0f);
	ImGui::EndDisabled();
	const bool ray_guiding_changed = ImGui::Checkbox("Enable ray guiding (experimental)", &state.enable_ray_guiding);
	result |= ray_guiding_changed;
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	if (path_length_changed || ray_guiding_changed) {
		vkDeviceWaitIdle(vk::context().device);
		destroy(integrator, /*resize=*/false);
		init(integrator);
	}
	return result;
}

}  // namespace vcm
