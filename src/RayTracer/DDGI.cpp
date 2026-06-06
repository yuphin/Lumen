#include <random>
#include "Integrator.h"
#include "Framework/VkUtils.h"
#include "DDGI.h"
constexpr i32 IRRADIANCE_SIDE_LENGTH = 8;
constexpr i32 DEPTH_SIDE_LENGTH = 16;

static void generate_uv_sphere(lm::Array<u32>& indices, lm::Array<SphereVertex>& positions, u32 latitude, u32 longitude,
							   f32 radius = 0.1f) {
	for (u32 lat = 0; lat <= latitude; ++lat) {
		f32 theta = lat * glm::pi<f32>() / latitude;
		f32 sin_theta = sin(theta);
		f32 cos_theta = cos(theta);

		for (u32 lon = 0; lon <= longitude; ++lon) {
			f32 phi = lon * 2.0f * glm::pi<f32>() / longitude;
			f32 sin_phi = sin(phi);
			f32 cos_phi = cos(phi);

			auto& vertex = positions.emplace_back();
			vertex.pos.x = radius * sin_theta * cos_phi;
			vertex.pos.y = radius * cos_theta;
			vertex.pos.z = radius * sin_theta * sin_phi;
			vertex.normal = glm::normalize(vertex.pos);
		}
	}

	for (u32 lat = 0; lat < latitude; ++lat) {
		for (u32 lon = 0; lon < longitude; ++lon) {
			u32 first = (lat * (longitude + 1)) + lon;
			u32 second = first + longitude + 1;

			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);

			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}
}

void ddgi::init(Integrator* integrator) {
	DDGI& state = integrator->ddgi;

	u32 num_probes;
	// DDGI Resources
	{
		glm::vec3 min_pos = integrator->lumen_scene->dimensions.min - vec3(0.1f);
		glm::vec3 max_pos = integrator->lumen_scene->dimensions.max + vec3(0.1f);
		glm::vec3 diag = (max_pos - min_pos) * 1.1f;
		state.probe_counts = glm::ivec3(diag / state.probe_distance);
		state.probe_start_position = min_pos;
		glm::vec3 bbox_div_probes = diag / glm::vec3(state.probe_counts);
		state.max_distance = bbox_div_probes.length() * 1.5f;

		// Samplers
		{
			VkSamplerCreateInfo sampler_ci = vk::sampler();
			sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_ci.magFilter = VK_FILTER_LINEAR;
			sampler_ci.minFilter = VK_FILTER_LINEAR;
			sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			sampler_ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			sampler_ci.compareEnable = VK_FALSE;
			sampler_ci.compareOp = VK_COMPARE_OP_ALWAYS;
			sampler_ci.minLod = 0.f;
			sampler_ci.maxLod = FLT_MAX;
			vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &state.bilinear_sampler));
			sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_ci.minFilter = VK_FILTER_NEAREST;
			sampler_ci.magFilter = VK_FILTER_NEAREST;
			vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &state.nearest_sampler));
		}

		const u32 irradiance_width = (IRRADIANCE_SIDE_LENGTH + 2) * state.probe_counts.x * state.probe_counts.y;
		const u32 irradiance_height = (IRRADIANCE_SIDE_LENGTH + 2) * state.probe_counts.z;
		const u32 depth_width = (DEPTH_SIDE_LENGTH + 2) * state.probe_counts.x * state.probe_counts.y;
		const u32 depth_height = (DEPTH_SIDE_LENGTH + 2) * state.probe_counts.z;
		// Irradiance and depth
		num_probes = state.probe_counts.x * state.probe_counts.y * state.probe_counts.z;

		// Debug visualization data
		if (!state.sphere_vertices.initialized()) {
			state.sphere_vertices = lm::array_create<SphereVertex>(integrator->arena, 512);
			state.sphere_indices = lm::array_create<u32>(integrator->arena, 2048);
			generate_uv_sphere(state.sphere_indices, state.sphere_vertices, 16, 16, 0.05f);
		}
		state.sphere_vertices_buffer = prm::get_buffer({
			.name = CSTR("Sphere Vertex Buffer"),
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
					 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			.memory_type = vk::BUFFER_TYPE_GPU,
			.size = sizeof(SphereVertex) * state.sphere_vertices.size,
			.data = state.sphere_vertices.data,
		});

		state.sphere_indices_buffer = prm::get_buffer({
			.name = CSTR("Sphere Index Buffer"),
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
					 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
			.memory_type = vk::BUFFER_TYPE_GPU,
			.size = sizeof(u32) * state.sphere_indices.size,
			.data = state.sphere_indices.data,
		});

		SphereDesc sphere_desc;
		sphere_desc.index_addr = state.sphere_indices_buffer->device_address();
		sphere_desc.vertex_addr = state.sphere_vertices_buffer->device_address();

		state.sphere_desc_buffer =
			prm::get_buffer({.name = CSTR("Sphere Desc"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = sizeof(SphereDesc),
							 .data = &sphere_desc});

		for (i32 i = 0; i < 2; i++) {
			lm::String tex_name = lm::str_concat(integrator->arena, "DDGI Irradiance ",
												 lm::str_from_u64(integrator->arena, i), /*cstr=*/true);
			state.irr_texes[i] = prm::get_texture({
				.name = tex_name,
				.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.dimensions = {irradiance_width, irradiance_height, 1},
				.format = VK_FORMAT_R16G16B16A16_SFLOAT,
				.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
				.sampler = state.bilinear_sampler,
			});

			tex_name = lm::str_concat(integrator->arena, "DDGI Depth ", lm::str_from_u64(integrator->arena, i),
									  /*cstr=*/true);

			state.depth_texes[i] = prm::get_texture({
				.name = tex_name,
				.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.dimensions = {depth_width, depth_height, 1},
				.format = VK_FORMAT_R16G16_SFLOAT,
				.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
				.sampler = state.bilinear_sampler,
			});
		}
		// RT
		create_radiance_textures(integrator);
		// DDGI Output
		state.output.tex = prm::get_texture({
			.name = CSTR("DDGI Output"),
			.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.dimensions = {Window::width(), Window::height(), 1},
			.format = VK_FORMAT_R16G16B16A16_SFLOAT,
			.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
			.sampler = state.bilinear_sampler,
		});
	}
	state.g_buffer = prm::get_buffer({
		.name = CSTR("GBuffer"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = Window::width() * Window::height() * sizeof(GBufferData),
	});

	state.direct_lighting_buffer = prm::get_buffer({
		.name = CSTR("Direct Lighting"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = Window::width() * Window::height() * sizeof(glm::vec3),
	});

	state.ddgi_ubo_buffer = prm::get_buffer({
		.name = CSTR("DDGI UBO"),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.memory_type = vk::BUFFER_TYPE_CPU_TO_GPU,
		.size = sizeof(DDGIUniforms),
	});

	state.probe_offsets_buffer = prm::get_buffer({
		.name = CSTR("Probe Offsets"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = sizeof(vec4) * num_probes,
	});

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	// DDGI
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	desc.direct_lighting_addr = state.direct_lighting_buffer->device_address();
	desc.probe_offsets_addr = state.probe_offsets_buffer->device_address();
	desc.g_buffer_addr = state.g_buffer->device_address();

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, direct_lighting_addr, state.direct_lighting_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, probe_offsets_addr, state.probe_offsets_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, g_buffer_addr, state.g_buffer, vk::render_graph());

	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	update_ddgi_uniforms(integrator);
	state.pc.total_light_area = 0;

	integrator->frame_num = 0;
}

void ddgi::render(Integrator* integrator) {
	DDGI& state = integrator->ddgi;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.time = rand() % UINT_MAX;
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.first_frame = state.first_frame;
	state.pc.infinite_bounces = state.infinite_bounces;
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.light_triangle_count = integrator->lumen_scene->total_light_triangle_cnt;
	state.pc.frame_num = integrator->frame_num;
	state.pc.direct_lighting = state.direct_lighting;
	const bool ping_pong = bool(state.frame_idx % 2);  // ping_pong true = read
	// Generate random orientation for probes
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> dis(-1.0, 1.0);
		glm::vec4 rands(0.5 * dis(gen) + 0.5, dis(gen), dis(gen), dis(gen));
		state.pc.probe_rotation = glm::mat4_cast(
			glm::angleAxis(2.0f * glm::pi<f32>() * rands.x, glm::normalize(glm::vec3(rands.y, rands.z, rands.w))));
	}
	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};
	// Trace Primary rays and fill G buffer
	vk::render_graph()
		->add_rt(CSTR("DDGI - GBuffer Pass"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/ddgi/primary_rays.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .specialization_data = {1},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.zero(state.g_buffer)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Trace rays from probes
	u32 grid_size = state.probe_counts.x * state.probe_counts.y * state.probe_counts.z;
	vk::render_graph()
		->add_rt(CSTR("DDGI - Probe Trace"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/ddgi/trace.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .specialization_data = {1},
					 .dims = {(u32)state.rays_per_probe, grid_size},
				 })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind({integrator->lumen_scene->mesh_lights_buffer, state.ddgi_ubo_buffer, state.rt.radiance_tex,
			   state.rt.dir_depth_tex, state.irr_texes[ping_pong], state.depth_texes[ping_pong]})
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Classify
	u32 wg_x = (state.probe_counts.x * state.probe_counts.y * state.probe_counts.z + 31) / 32;
	vk::render_graph()
		->add_compute(CSTR("Classify Probes"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/ddgi/classify.comp")), .dims = {wg_x}})
		.push_constants(&state.pc)
		.bind({integrator->scene_ubo_buffer, integrator->lumen_scene->scene_desc_buffer, state.ddgi_ubo_buffer,
			   state.rt.radiance_tex, state.rt.dir_depth_tex});
	// Update probes & borders
	{
		// Probes
		wg_x = state.probe_counts.x * state.probe_counts.y;
		u32 wg_y = state.probe_counts.z;
		auto update_probe = [&](bool is_irr) {
			lm::String pipeline_name = is_irr ? CSTR("Update Irradiance") : CSTR("Update Depth");
			vk::render_graph()
				->add_compute(pipeline_name, {.shader = vk::Shader(CSTR("src/shaders/integrators/ddgi/update.comp")),
											  .macros = {is_irr ? vk::ShaderMacro("IRRADIANCE_UPDATE")
																: vk::ShaderMacro("DEPTH_UPDATE")},
											  .dims = {wg_x, wg_y}})
				.push_constants(&state.pc)
				.bind({integrator->lumen_scene->scene_desc_buffer, state.irr_texes[!ping_pong],
					   state.depth_texes[!ping_pong], state.irr_texes[ping_pong], state.depth_texes[ping_pong],
					   state.ddgi_ubo_buffer, state.rt.radiance_tex, state.rt.dir_depth_tex});
		};
		update_probe(true);
		update_probe(false);
		// Borders
		// 13 WGs process 4 probes (wg = 32 threads)
		wg_x = (state.probe_counts.x * state.probe_counts.y * state.probe_counts.z + 3) * 13 / 4;
		vk::render_graph()
			->add_compute(
				CSTR("Update Borders"),
				{.shader = vk::Shader(CSTR("src/shaders/integrators/ddgi/update_borders.comp")), .dims = {wg_x}})
			.push_constants(&state.pc)
			.bind({state.irr_texes[!ping_pong], state.depth_texes[!ping_pong], state.ddgi_ubo_buffer});
	}
	// Sample probes & output into texture
	wg_x = (Window::width() + 31) / 32;
	u32 wg_y = (Window::height() + 31) / 32;
	vk::render_graph()
		->add_compute(CSTR("Sample Probes"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/ddgi/sample.comp")), .dims = {wg_x, wg_y}})
		.push_constants(&state.pc)
		.bind({integrator->scene_ubo_buffer, integrator->lumen_scene->scene_desc_buffer, state.output.tex,
			   state.irr_texes[!ping_pong], state.depth_texes[!ping_pong], state.ddgi_ubo_buffer,
			   integrator->output_tex});

	if (state.visualize_probes) {
		vk::render_graph()
			->add_rt(CSTR("Visualize probes"),
					 {
						 .shaders = {{CSTR("src/shaders/integrators/ddgi/probe_vis.rgen")},
									 {CSTR("src/shaders/integrators/ddgi/probe_vis.rmiss")},
									 {CSTR("src/shaders/integrators/ddgi/probe_vis.rchit")},
									 {CSTR("src/shaders/ray.rahit")}},
						 .dims = {Window::width(), Window::height()},
					 })
			.push_constants(&state.pc)
			.bind({integrator->output_tex, integrator->scene_ubo_buffer, state.sphere_desc_buffer})
			.bind_tlas(*integrator->tlas);
	}
	// Relocate
	if (state.total_frame_idx < 5) {
		// 13 WGs process 4 probes (wg = 32 threads)
		wg_x = (state.probe_counts.x * state.probe_counts.y * state.probe_counts.z + 31) / 32;
		vk::render_graph()
			->add_compute(CSTR("Relocate"),
						  {.shader = vk::Shader(CSTR("src/shaders/integrators/ddgi/relocate.comp")), .dims = {wg_x}})
			.push_constants(&state.pc)
			.bind({integrator->scene_ubo_buffer, integrator->lumen_scene->scene_desc_buffer, state.ddgi_ubo_buffer,
				   state.rt.dir_depth_tex});
	}
	state.first_frame = false;
}

bool ddgi::update(Integrator* integrator) {
	DDGI& state = integrator->ddgi;
	integrator->frame_num++;
	state.frame_idx++;
	state.total_frame_idx++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	update_ddgi_uniforms(integrator);
	return updated;
}
bool ddgi::gui(Integrator* integrator) {
	DDGI& state = integrator->ddgi;
	bool result = false;
	result |= ImGui::SliderFloat("Hysteresis", &state.hysteresis, 0.0f, 1.0f);
	bool rpp_changed = ImGui::SliderInt("Rays per probe", (i32*)&state.rays_per_probe, 1, 4096);
	if (rpp_changed && (state.rays_per_probe > 0)) {
		vkDeviceWaitIdle(vk::context().device);
		prm::remove(state.rt.radiance_tex);
		prm::remove(state.rt.dir_depth_tex);
		create_radiance_textures(integrator);
	}
	result |= rpp_changed;
	result |= ImGui::SliderFloat("Ray max distance", &state.tmax, 0.0f, 1000.0f);
	result |= ImGui::SliderFloat("Ray min distance", &state.tmin, 0.0f, 1000.0f);
	result |= ImGui::SliderFloat("Normal bias", &state.normal_bias, 0.0f, 1.0f);
	result |= ImGui::Checkbox("Infinite bounces", &state.infinite_bounces);
	result |= ImGui::Checkbox("Direct lighting", &state.direct_lighting);
	result |= ImGui::Checkbox("Visualize probes", &state.visualize_probes);
	ImGui::Text("Probe dimensions: %dx%dx%d", state.probe_counts.x, state.probe_counts.y, state.probe_counts.z);
	ImGui::Text("Probe distance: %f", state.probe_distance);
	const u32 num_rays = state.rays_per_probe * state.probe_counts.x * state.probe_counts.y * state.probe_counts.z;
	ImGui::Text("Number of rays: %d", num_rays);
	ImGui::Text("Rays per pixel: %f", (f32)num_rays / (Window::width() * Window::height()));
	return result;
}

void ddgi::update_ddgi_uniforms(Integrator* integrator) {
	DDGI& state = integrator->ddgi;
	state.ddgi_ubo.probe_counts = state.probe_counts;
	state.ddgi_ubo.hysteresis = state.hysteresis;
	state.ddgi_ubo.probe_start_position = state.probe_start_position;
	state.ddgi_ubo.probe_step = state.probe_distance;
	state.ddgi_ubo.rays_per_probe = state.rays_per_probe;
	state.ddgi_ubo.max_distance = state.max_distance;
	state.ddgi_ubo.depth_sharpness = state.depth_sharpness;
	state.ddgi_ubo.normal_bias = state.normal_bias;
	state.ddgi_ubo.irradiance_width = state.irr_texes[0]->extent.width;
	state.ddgi_ubo.irradiance_height = state.irr_texes[0]->extent.height;
	state.ddgi_ubo.depth_width = state.depth_texes[0]->extent.width;
	state.ddgi_ubo.depth_height = state.depth_texes[0]->extent.height;
	state.ddgi_ubo.backface_ratio = state.backface_ratio;
	state.ddgi_ubo.min_frontface_dist = state.min_frontface_dist;
	state.ddgi_ubo.tmax = state.tmax;
	state.ddgi_ubo.tmin = state.tmin;
	vk::buffer_write(state.ddgi_ubo_buffer, &state.ddgi_ubo, sizeof(state.ddgi_ubo));
}

void ddgi::create_radiance_textures(Integrator* integrator) {
	DDGI& state = integrator->ddgi;
	u32 num_probes = state.probe_counts.x * state.probe_counts.y * state.probe_counts.z;
	state.rt.radiance_tex = prm::get_texture({
		.name = CSTR("DDGI Radiance"),
		.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.dimensions = {state.rays_per_probe, num_probes, 1},
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
		.sampler = state.nearest_sampler,
	});
	state.rt.dir_depth_tex = prm::get_texture({
		.name = CSTR("DDGI Radiance & Tex"),
		.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.dimensions = {state.rays_per_probe, num_probes, 1},
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
		.sampler = state.nearest_sampler,
	});
}

void ddgi::create_accel(Integrator* integrator, vk::BVH* tlas_ptr, lm::Array<vk::BVH>* blases_ptr) {
	DDGI& state = integrator->ddgi;
	vk::BVH& tlas = *tlas_ptr;
	lm::Array<vk::BVH>& blases = *blases_ptr;
	if (!blases.initialized()) {
		blases = lm::array_create<vk::BVH>(integrator->arena, integrator->lumen_scene->prim_meshes.size);
	}

	// + 1 for the sphere
	u64 blas_inputs_size = integrator->lumen_scene->prim_meshes.size + 1;
	blases.resize_with_value(blas_inputs_size);

	lm::ScratchArena scratch = integrator->arena;
	auto blas_inputs = lm::fixed_array_create<vk::BlasInput>(scratch.arena, blas_inputs_size);

	VkDeviceAddress vertex_address = integrator->lumen_scene->vertex_buffer->device_address();
	VkDeviceAddress idx_address = integrator->lumen_scene->index_buffer->device_address();
	for (auto& prim_mesh : integrator->lumen_scene->prim_meshes) {
		vk::BlasInput geo = vk::blas_input_create(prim_mesh.vtx_count, prim_mesh.idx_count, prim_mesh.vtx_offset,
												  prim_mesh.first_idx, vertex_address, sizeof(Vertex), idx_address);
		blas_inputs.push_back({geo});
	}

	{
		VkDeviceAddress sphere_vertex_addr = state.sphere_vertices_buffer->device_address();
		VkDeviceAddress sphere_idx_addr = state.sphere_indices_buffer->device_address();

		VkAccelerationStructureGeometryTrianglesDataKHR sphere_triangles{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
		sphere_triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		sphere_triangles.vertexData.deviceAddress = sphere_vertex_addr;
		sphere_triangles.maxVertex = u32(state.sphere_vertices.size);
		sphere_triangles.vertexStride = sizeof(SphereVertex);
		sphere_triangles.indexType = VK_INDEX_TYPE_UINT32;
		sphere_triangles.indexData.deviceAddress = sphere_idx_addr;
		sphere_triangles.transformData.deviceAddress = 0;  // No per-geometry transform

		VkAccelerationStructureBuildRangeInfoKHR offset;
		offset.firstVertex = 0;
		offset.primitiveCount = u32(state.sphere_indices.size) / 3;
		offset.primitiveOffset = 0;
		offset.transformOffset = 0;

		VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeom.geometry.triangles = sphere_triangles;

		vk::BlasInput sphere_blas_input = {.geometry = asGeom, .build_range = offset};
		blas_inputs.push_back(sphere_blas_input);
	}

	vk::blas_build(scratch, blases.to_slice(), blas_inputs.to_slice(),
				   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

	u32 num_probes = state.probe_counts.x * state.probe_counts.y * state.probe_counts.z;
	auto tlas_instances = lm::fixed_array_create<VkAccelerationStructureInstanceKHR>(
		scratch.arena, integrator->lumen_scene->prim_meshes.size + num_probes);
	for (const auto& pm : integrator->lumen_scene->prim_meshes) {
		VkAccelerationStructureInstanceKHR ray_inst{};
		ray_inst.transform = vk::to_vk_matrix(pm.world_matrix);
		ray_inst.instanceCustomIndex = pm.prim_idx;
		assert(pm.prim_idx < blases.size);
		ray_inst.accelerationStructureReference = blases[pm.prim_idx].device_address();
		ray_inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		ray_inst.mask = 0x1;
		ray_inst.instanceShaderBindingTableRecordOffset = 0;
		tlas_instances.push_back(ray_inst);
	}

	{
		const u32 sphere_blas_idx = static_cast<u32>(blases.size) - 1;
		for (u32 i = 0; i < num_probes; ++i) {
			VkAccelerationStructureInstanceKHR sphere_inst{};

			glm::vec3 position = probe_location(integrator, i);
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);

			sphere_inst.transform = vk::to_vk_matrix(transform);
			sphere_inst.instanceCustomIndex = i;
			sphere_inst.accelerationStructureReference = blases[sphere_blas_idx].device_address();
			sphere_inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			sphere_inst.mask = 0x2;
			sphere_inst.instanceShaderBindingTableRecordOffset = 0;
			tlas_instances.push_back(sphere_inst);
		}
	}
	vk::tlas_build(tlas, tlas_instances.to_slice(), VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

glm::vec3 ddgi::probe_location(Integrator* integrator, u32 index) {
	DDGI& state = integrator->ddgi;
	glm::ivec3 grid_coord = probe_index_to_grid_coord(integrator, index);
	glm::vec3 grid_pos = grid_coord_to_position(integrator, grid_coord);
	// TODO: Add offsets
	return grid_pos;
}

glm::ivec3 ddgi::probe_index_to_grid_coord(Integrator* integrator, u32 index) {
	DDGI& state = integrator->ddgi;
	glm::ivec3 res;
	res.x = index % state.probe_counts.x;
	res.y = (index / state.probe_counts.x) % state.probe_counts.y;
	res.z = index / (state.probe_counts.x * state.probe_counts.y);
	return res;
}

glm::vec3 ddgi::grid_coord_to_position(Integrator* integrator, const glm::ivec3& grid_coord) {
	DDGI& state = integrator->ddgi;
	return glm::vec3(grid_coord) * state.probe_distance + state.probe_start_position;
}

void ddgi::destroy(Integrator* integrator, bool resize) {
	DDGI& state = integrator->ddgi;
	(void)resize;

	vk::Buffer** buffers[] = {&state.g_buffer,
							  &state.direct_lighting_buffer,
							  &state.ddgi_ubo_buffer,
							  &state.probe_offsets_buffer,
							  &state.sphere_vertices_buffer,
							  &state.sphere_indices_buffer,
							  &state.sphere_desc_buffer,
							  &state.ddgi_output_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}

	vk::Texture** textures[] = {&state.rt.radiance_tex, &state.rt.dir_depth_tex, &state.output.tex};
	for (vk::Texture** texture : textures) {
		prm::remove(*texture);
		*texture = nullptr;
	}

	vkDestroySampler(vk::context().device, state.bilinear_sampler, nullptr);
	vkDestroySampler(vk::context().device, state.nearest_sampler, nullptr);
	state.bilinear_sampler = VK_NULL_HANDLE;
	state.nearest_sampler = VK_NULL_HANDLE;

	for (vk::Texture*& irr_tex : state.irr_texes) {
		prm::remove(irr_tex);
		irr_tex = nullptr;
	}

	for (vk::Texture*& depth_tex : state.depth_texes) {
		prm::remove(depth_tex);
		depth_tex = nullptr;
	}
}
