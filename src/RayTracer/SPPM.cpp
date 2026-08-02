#include "Integrator.h"
#include "SPPM.h"

namespace sppm {

void init(Integrator* integrator) {
	SPPM& state = integrator->sppm;


	state.sppm_data_buffer =
		prm::get_buffer({.name = CSTR("SPPM Data"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(SPPMData)});

	state.atomic_data_buffer =
		prm::get_buffer({.name = CSTR("Atomic Data"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(AtomicData)});

	state.photon_buffer =
		prm::get_buffer({.name = CSTR("Photon Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = 10 * Window::width() * Window::height() * sizeof(PhotonHash)});

	state.residual_buffer =
		prm::get_buffer({.name = CSTR("Residual Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * 4 * sizeof(f32)});

	state.counter_buffer =
		prm::get_buffer({.name = CSTR("Counter Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(i32)});

	SceneDesc desc = integrator::scene_desc_base(integrator);
	// SPPM
	SET_SCENE_BUFFER(desc, sppm_data, state.sppm_data_buffer);
	SET_SCENE_BUFFER(desc, atomic_data, state.atomic_data_buffer);
	SET_SCENE_BUFFER(desc, photon, state.photon_buffer);
	SET_SCENE_BUFFER(desc, residual, state.residual_buffer);
	SET_SCENE_BUFFER(desc, counter, state.counter_buffer);
	integrator::upload_scene_desc(integrator, desc);

	integrator->frame_num = 0;

	assert(rg::settings().shader_inference == true);
}

void render(Integrator* integrator) {
	SPPM& state = integrator->sppm;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = i32(integrator->lumen_scene->gpu_lights.size);
	state.pc.time = lm::rand_u32();
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.random_num = lm::rand_u32();
	state.pc.frame_num = integrator->frame_num;
	state.pc.enable_accumulation = state.enable_accumulation;
	SPPMConfig& config = integrator->lumen_scene->config.settings.sppm;
	// PPM related constants
	if (config.base_radius < 1e-7f) {
		config.base_radius = 1e-7f;
	}
	state.pc.min_bounds = integrator->lumen_scene->dimensions.min;
	state.pc.max_bounds = integrator->lumen_scene->dimensions.max;
	state.pc.ppm_base_radius = config.base_radius;
	const lm::vec3 diam = state.pc.max_bounds - state.pc.min_bounds;
	const f32 max_comp = lm::max(diam.x, lm::max(diam.y, diam.z));
	const i32 base_grid_res = i32(max_comp / config.base_radius);
	state.pc.grid_res = lm::max(ivec3(diam * f32(base_grid_res) / max_comp), ivec3(1));
	auto op_reduce = [&](const lm::String& op_name, const lm::String& op_shader_name, const lm::String& reduce_name,
						 const lm::String& reduce_shader_name) {
		u32 num_wgs = u32((Window::width() * Window::height() + 1023) / 1024);
		rg::add_compute(op_name, {.shader = vk::Shader(op_shader_name), .dims = {num_wgs, 1, 1}})
			.push_constants(&state.pc)
			.bind(integrator->lumen_scene->scene_desc_buffer)
			.zero({state.residual_buffer, state.counter_buffer});
		while (num_wgs != 1) {
			rg::add_compute(reduce_name, {.shader = vk::Shader(reduce_shader_name), .dims = {num_wgs, 1, 1}})
				.push_constants(&state.pc)
				.bind(integrator->lumen_scene->scene_desc_buffer);
			num_wgs = (u32)(num_wgs + 1023) / 1024;
		}
	};

	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};

	// Trace rays from eye
	rg::add_rt(CSTR("SPPM - Eye"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/sppm/sppm_eye.rgen")},
								 {CSTR("src/shaders/surface.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/surface.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.zero(state.photon_buffer)
		.zero(state.sppm_data_buffer, /*cond=*/state.pc.frame_num == 0)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Calculate scene bbox given the calculated radius
	op_reduce(CSTR("OpReduce: Max"), CSTR("src/shaders/integrators/sppm/max.comp"), CSTR("OpReduce: Reduce Max"),
			  CSTR("src/shaders/integrators/sppm/reduce_max.comp"));
	op_reduce(CSTR("OpReduce: Min"), CSTR("src/shaders/integrators/sppm/min.comp"), CSTR("OpReduce: Reduce Min"),
			  CSTR("src/shaders/integrators/sppm/reduce_min.comp"));
	rg::add_compute(CSTR("Bounds Calculation"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/sppm/calc_bounds.comp")), .dims = {1, 1, 1}})
		.bind(integrator->lumen_scene->scene_desc_buffer);
	// Trace from light
	rg::add_rt(CSTR("SPPM - Light"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/sppm/sppm_light.rgen")},
								 {CSTR("src/shaders/surface.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/surface.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Gather
	rg::add_compute(CSTR("Gather"), {.shader = vk::Shader(CSTR("src/shaders/integrators/sppm/gather.comp")),
								 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures);
	// Composite
	rg::add_compute(CSTR("Composite"), {.shader = vk::Shader(CSTR("src/shaders/integrators/sppm/composite.comp")),
									.dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind({integrator->output_tex, integrator->lumen_scene->scene_desc_buffer});
}

bool update(Integrator* integrator) {
	SPPM& state = integrator->sppm;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

bool gui(Integrator* integrator) {
	SPPM& state = integrator->sppm;
	return ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
}

void destroy(Integrator* integrator, bool resize) {
	SPPM& state = integrator->sppm;
	(void)resize;

	vk::Buffer** buffers[] = {&state.sppm_data_buffer, &state.atomic_data_buffer, &state.photon_buffer,
							 &state.residual_buffer, &state.counter_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}

	if (state.desc_set_layout) vkDestroyDescriptorSetLayout(vk::context().device, state.desc_set_layout, nullptr);
	if (state.desc_pool) vkDestroyDescriptorPool(vk::context().device, state.desc_pool, nullptr);
	state.desc_set_layout = VK_NULL_HANDLE;
	state.desc_pool = VK_NULL_HANDLE;
}

}  // namespace sppm
