#include "LumenPCH.h"
#include "ReSTIRPT.h"

void ReSTIRPT::init() {
	Integrator::init();

	std::vector<glm::mat4> transformations;
	transformations.resize(lumen_scene->prim_meshes.size());
	for (auto& pm : lumen_scene->prim_meshes) {
		transformations[pm.prim_idx] = pm.world_matrix;
	}

	gris_gbuffer =
		prm::get_buffer({.name = "GRIS GBuffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(GBuffer)});

	gris_prev_gbuffer =
		prm::get_buffer({.name = "GRIS Previous GBuffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(GBuffer)});

	direct_lighting_texture = prm::get_texture({.name = "Direct Lighting Texture",
												.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
												.dimensions = {Window::width(), Window::height(), 1},
												.format = VK_FORMAT_R32G32B32A32_SFLOAT});
	gris_reservoir_ping_buffer =
		prm::get_buffer({.name = "GRIS Reservoirs Ping",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(Reservoir)});

	gris_reservoir_pong_buffer =
		prm::get_buffer({.name = "GRIS Reservoirs Pong",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(Reservoir)});

	prefix_contribution_buffer =
		prm::get_buffer({.name = "Prefix Contributions",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(glm::vec3)});

	debug_vis_buffer =
		prm::get_buffer({.name = "Debug Vis",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(uint32_t)});
	reconnection_buffer = prm::get_buffer(
		{.name = "Reservoir Connection",
		 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		 .memory_type = vk::BufferType::GPU,
		 .size = Window::width() * Window::height() * sizeof(ReconnectionData) * (num_spatial_samples + 1)});

	transformations_buffer = prm::get_buffer({
		.name = "Transformations Buffer",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = transformations.size() * sizeof(glm::mat4),
		.data = transformations.data(),
	});
	photon_eye_buffer =
		prm::get_buffer({.name = "Photon - Eye",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(PhotonEyeData)});

	caustic_photon_aabbs_buffer =
		prm::get_buffer({.name = "Caustic Photon AABBs",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(float) * 6});
	caustic_photon_light_buffer =
		prm::get_buffer({.name = "Caustic Photon - Light",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height() * sizeof(PhotonLightData)});
	photon_count_buffer =
		prm::get_buffer({.name = "Photon Counts",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = 4});
	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->get_device_address();

	desc.material_addr = lumen_scene->materials_buffer->get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	// ReSTIR PT (GRIS)
	desc.transformations_addr = transformations_buffer->get_device_address();
	desc.prefix_contributions_addr = prefix_contribution_buffer->get_device_address();
	desc.debug_vis_addr = debug_vis_buffer->get_device_address();
	desc.photon_eye_addr = photon_eye_buffer->get_device_address();
	desc.caustic_photon_aabbs_addr = caustic_photon_aabbs_buffer->get_device_address();
	desc.caustic_photon_light_addr = caustic_photon_light_buffer->get_device_address();
	desc.photon_count_addr = photon_count_buffer->get_device_address();

	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	canonical_contributions_texture = prm::get_texture({
		.name = "Canonical Contributions Texture",
		.usage = VK_IMAGE_USAGE_STORAGE_BIT,
		.dimensions = {Window::width(), Window::height(), 1},
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
	});

	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.total_frame_num = 0;
	pc_ray.buffer_idx = 0;

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_reservoir_addr, gris_reservoir_ping_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, compact_vertices_addr, lumen_scene->compact_vertices_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, debug_vis_addr, debug_vis_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_eye_addr, photon_eye_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, caustic_photon_aabbs_addr, caustic_photon_aabbs_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, caustic_photon_light_addr, caustic_photon_light_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_count_addr, photon_count_buffer, vk::render_graph());

	path_length = config->path_length;
}

void ReSTIRPT::render() {
	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
	pc_ray.enable_temporal_jitter = uint(enable_temporal_jitter);
	pc_ray.num_lights = (int)lumen_scene->gpu_lights.size();
	pc_ray.prev_random_num = pc_ray.general_seed;
	pc_ray.sampling_seed = rand() % UINT_MAX;
	pc_ray.seed2 = rand() % UINT_MAX;
	pc_ray.seed3 = rand() % UINT_MAX;
	pc_ray.max_depth = path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.dir_light_idx = lumen_scene->dir_light_idx;
	pc_ray.enable_accumulation = enable_accumulation;
	pc_ray.num_spatial_samples = num_spatial_samples;
	pc_ray.scene_extent = glm::length(lumen_scene->m_dimensions.max - lumen_scene->m_dimensions.min);
	pc_ray.direct_lighting = direct_lighting;
	pc_ray.enable_rr = enable_rr;

	pc_ray.spatial_radius = spatial_reuse_radius;
	pc_ray.enable_spatial_reuse = enable_spatial_reuse;
	pc_ray.hide_reconnection_radiance = hide_reconnection_radiance;
	pc_ray.min_vertex_distance_ratio = min_vertex_distance_ratio;
	pc_ray.enable_gris = enable_gris;
	pc_ray.frame_num = frame_num;
	pc_ray.pixel_debug = pixel_debug;
	pc_ray.temporal_reuse = uint(enable_temporal_reuse);
	pc_ray.permutation_sampling = uint(enable_permutation_sampling);
	pc_ray.gris_separator = gris_separator;
	pc_ray.canonical_only = canonical_only;
	pc_ray.enable_occlusion = enable_occlusion;
	pc_ray.photon_radius = photon_radius;

	const std::initializer_list<lumen::ResourceBinding> common_bindings = {
		output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer, lumen_scene->mesh_lights_buffer};

	const std::array<vk::Buffer*, 2> reservoir_buffers = {gris_reservoir_ping_buffer, gris_reservoir_pong_buffer};
	const std::array<vk::Buffer*, 2> gbuffers = {gris_prev_gbuffer, gris_gbuffer};

	int ping = pc_ray.total_frame_num % 2;
	int pong = ping ^ 1;

	constexpr int WRITE_OR_CURR_IDX = 1;
	constexpr int READ_OR_PREV_IDX = 0;

	if (enable_photon_mapping) {
		vk::render_graph()
			->add_rt("PM - Trace First Diffuse",
					 {
						 .shaders = {{"src/shaders/integrators/restir/gris/pm_trace_eye.rgen"},
									 {"src/shaders/integrators/restir/gris/ray.rmiss"},
									 {"src/shaders/ray_shadow.rmiss"},
									 {"src/shaders/integrators/restir/gris/ray.rchit"},
									 {"src/shaders/ray.rahit"}},
						 .macros = {vk::ShaderMacro("ENABLE_ATMOSPHERE", enable_atmosphere)},
						 .dims = {Window::width(), Window::height()},
					 })
			.push_constants(&pc_ray)
			.bind(common_bindings)
			.bind_texture_array(lumen_scene->scene_textures)
			.bind_as(tlas);

		VkAccelerationStructureGeometryAabbsDataKHR aabbs_data{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR};
		aabbs_data.data.deviceAddress = caustic_photon_aabbs_buffer->get_device_address();

		VkAccelerationStructureBuildRangeInfoKHR offset;
		offset.primitiveCount = num_photons;

		VkAccelerationStructureGeometryKHR as_geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		as_geom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
		as_geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		as_geom.geometry.aabbs = aabbs_data;

		std::vector<vk::BlasInput> photon_blas_inputs;
		vk::BlasInput& photon_blas_input = photon_blas_inputs.emplace_back();
		photon_blas_input.as_geom.push_back(as_geom);
		photon_blas_input.as_build_offset_info.push_back(offset);

		vk::render_graph()
			->add_rt("PM - Trace Photons",
					 {
						 .shaders = {{"src/shaders/integrators/restir/gris/pm_trace_photons.rgen"},
									 {"src/shaders/integrators/restir/gris/ray.rmiss"},
									 {"src/shaders/ray_shadow.rmiss"},
									 {"src/shaders/integrators/restir/gris/ray.rchit"},
									 {"src/shaders/ray.rahit"}},
						 .macros = {vk::ShaderMacro("ENABLE_ATMOSPHERE", enable_atmosphere)},
						 .dims = {num_photons, 1},
					 })
			.push_constants(&pc_ray)
			.bind(common_bindings)
			.bind_texture_array(lumen_scene->scene_textures)
			.zero(photon_count_buffer)
			.zero(caustic_photon_aabbs_buffer)
			.build_blas(photon_bvh, photon_blas_inputs, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
						{caustic_photon_aabbs_buffer}, &photon_bvh_scratch_buf)
			.bind_as(tlas);
	}

	// Trace rays
	vk::render_graph()
		->add_rt("GRIS - Generate Samples",
				 {
					 .shaders = {{"src/shaders/integrators/restir/gris/gris.rgen"},
								 {"src/shaders/integrators/restir/gris/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/integrators/restir/gris/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .macros = {{"STREAMING_MODE", int(streaming_method)},
								vk::ShaderMacro("ENABLE_ATMOSPHERE", enable_atmosphere)},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&pc_ray)
		.zero(debug_vis_buffer)
		.bind(common_bindings)
		.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
		.bind(gbuffers[pong])
		.bind(canonical_contributions_texture)
		.bind(direct_lighting_texture)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_as(tlas);

	pc_ray.general_seed = rand() % UINT_MAX;
	if (enable_gris) {
		bool should_do_temporal = enable_temporal_reuse && pc_ray.total_frame_num > 0;
		// Temporal Reuse
		vk::render_graph()
			->add_rt("GRIS - Temporal Reuse",
					 {
						 .shaders = {{"src/shaders/integrators/restir/gris/temporal_reuse.rgen"},
									 {"src/shaders/integrators/restir/gris/ray.rmiss"},
									 {"src/shaders/ray_shadow.rmiss"},
									 {"src/shaders/integrators/restir/gris/ray.rchit"},
									 {"src/shaders/ray.rahit"}},
						 .dims = {Window::width(), Window::height()},
					 })
			.push_constants(&pc_ray)
			.bind(common_bindings)
			.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
			.bind(reservoir_buffers[READ_OR_PREV_IDX])
			.bind(gbuffers[pong])
			.bind(gbuffers[ping])
			.bind(canonical_contributions_texture)
			.bind_texture_array(lumen_scene->scene_textures)
			.bind_as(tlas)
			.skip_execution(!should_do_temporal);
		pc_ray.seed2 = rand() % UINT_MAX;
		if (!canonical_only) {
			if (mis_method == MISMethod::TALBOT) {
				vk::render_graph()
					->add_rt("GRIS - Spatial Reuse - Talbot",
							 {
								 .shaders = {{"src/shaders/integrators/restir/gris/spatial_reuse_talbot.rgen"},
											 {"src/shaders/integrators/restir/gris/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/integrators/restir/gris/ray.rchit"},
											 {"src/shaders/ray.rahit"}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(reservoir_buffers[READ_OR_PREV_IDX])
					.bind(gbuffers[pong])
					.bind(canonical_contributions_texture)
					.bind(direct_lighting_texture)
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_as(tlas);
			} else {
				// Retrace
				vk::render_graph()
					->add_rt("GRIS - Retrace Reservoirs",
							 {
								 .shaders = {{"src/shaders/integrators/restir/gris/retrace_paths.rgen"},
											 {"src/shaders/integrators/restir/gris/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/integrators/restir/gris/ray.rchit"},
											 {"src/shaders/ray.rahit"}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(gbuffers[pong])
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_as(tlas);
				// Validate
				vk::render_graph()
					->add_rt("GRIS - Validate Samples",
							 {
								 .shaders = {{"src/shaders/integrators/restir/gris/validate_samples.rgen"},
											 {"src/shaders/integrators/restir/gris/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/integrators/restir/gris/ray.rchit"},
											 {"src/shaders/ray.rahit"}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(gbuffers[pong])
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_as(tlas);

				// Spatial Reuse
				vk::render_graph()
					->add_rt(
						"GRIS - Spatial Reuse",
						{
							.shaders = {{"src/shaders/integrators/restir/gris/spatial_reuse.rgen"},
										{"src/shaders/integrators/restir/gris/ray.rmiss"},
										{"src/shaders/ray_shadow.rmiss"},
										{"src/shaders/integrators/restir/gris/ray.rchit"},
										{"src/shaders/ray.rahit"}},
							.macros = {vk::ShaderMacro("ENABLE_DEFENSIVE_PAIRWISE_MIS", enable_defensive_formulation)},
							.dims = {Window::width(), Window::height()},
						})
					.push_constants(&pc_ray)
					.bind(common_bindings)
					.bind(reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(reservoir_buffers[READ_OR_PREV_IDX])
					.bind(gbuffers[pong])
					.bind(canonical_contributions_texture)
					.bind(direct_lighting_texture)
					.bind_texture_array(lumen_scene->scene_textures)
					.bind_as(tlas);
			}
			if (pixel_debug || (gris_separator < 1.0f && gris_separator > 0.0f)) {
				uint32_t num_wgs = uint32_t((Window::width() * Window::height() + 1023) / 1024);
				vk::render_graph()
					->add_compute(
						"GRIS - Debug Visualiation",
						{.shader = vk::Shader("src/shaders/integrators/restir/gris/debug_vis.comp"), .dims = {num_wgs}})
					.push_constants(&pc_ray)
					.bind({output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer});
			}
		}
	}
	pc_ray.total_frame_num++;
}

bool ReSTIRPT::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

void ReSTIRPT::destroy() {
	Integrator::destroy();
	auto buffer_list = {gris_gbuffer,
						gris_reservoir_ping_buffer,
						gris_reservoir_pong_buffer,
						transformations_buffer,
						prefix_contribution_buffer,
						reconnection_buffer,
						gris_prev_gbuffer,
						debug_vis_buffer,
						photon_eye_buffer,
						caustic_photon_aabbs_buffer,
						caustic_photon_light_buffer,
						photon_count_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
	prm::remove(canonical_contributions_texture);
	prm::remove(direct_lighting_texture);
}

bool ReSTIRPT::gui() {
	bool result = Integrator::gui();
	result |= ImGui::Checkbox("Direct lighting", &direct_lighting);
	result |= ImGui::Checkbox("Enable atmosphere", &enable_atmosphere);
	result |= ImGui::Checkbox("Enable Russian roulette", &enable_rr);
	result |= ImGui::Checkbox("Enable accumulation", &enable_accumulation);
	bool enable_gris_changed = ImGui::Checkbox("Enable GRIS", &enable_gris);
	result |= enable_gris_changed;
	if (enable_gris_changed) {
		pc_ray.total_frame_num = 0;
	}
	result |= ImGui::SliderInt("Path length", (int*)&path_length, 0, 12);
	result |= ImGui::Checkbox("Enable canonical-only mode", &canonical_only);
	if (!enable_gris) {
		return result;
	}
	int curr_streaming_method = static_cast<int>(streaming_method);
	std::array<const char*, 2> streaming_methods = {
		"Individual contributions",
		"Split at reconnection",
	};
	if (ImGui::Combo("Streaming method", &curr_streaming_method, streaming_methods.data(),
					 int(streaming_methods.size()))) {
		result = true;
		streaming_method = static_cast<StreamingMethod>(curr_streaming_method);
	}
	if (canonical_only) {
		return result;
	}
	result |= ImGui::SliderFloat("GRIS / Default", &gris_separator, 0.0f, 1.0f);
	result |= ImGui::Checkbox("Enable occlusion", &enable_occlusion);
	result |= ImGui::Checkbox("Temporal jitter", &enable_temporal_jitter);
	result |= ImGui::Checkbox("Debug pixels", &pixel_debug);
	result |= ImGui::Checkbox("Enable defensive formulation", &enable_defensive_formulation);
	result |= ImGui::Checkbox("Enable permutation sampling", &enable_permutation_sampling);
	result |= ImGui::Checkbox("Enable spatial reuse", &enable_spatial_reuse);
	result |= ImGui::Checkbox("Hide reconnection radiance", &hide_reconnection_radiance);
	std::array<const char*, 2> mis_methods = {
		"Talbot (Reconnection only)",
		"Pairwise",
	};
	int curr_mis_method = static_cast<int>(mis_method);
	if (ImGui::Combo("MIS method", &curr_mis_method, mis_methods.data(), int(mis_methods.size()))) {
		result = true;
		mis_method = static_cast<MISMethod>(curr_mis_method);
	}
	result |= ImGui::Checkbox("Enable temporal reuse", &enable_temporal_reuse);
	bool spatial_samples_changed = ImGui::SliderInt("Num spatial samples", (int*)&num_spatial_samples, 0, 12);
	result |= spatial_samples_changed;
	result |= ImGui::SliderFloat("Spatial radius", &spatial_reuse_radius, 0.0f, 128.0f);
	result |= ImGui::SliderFloat("Min reconnection distance ratio", &min_vertex_distance_ratio, 0.0f, 1.0f);

	result |= ImGui::Checkbox("Enable photon mapping", &enable_photon_mapping);
	if (enable_photon_mapping) {
		result |= ImGui::SliderInt("Num photons", (int*)&num_photons, 1, Window::width() * Window::height());
		result |= ImGui::SliderFloat("Photon radius", &photon_radius, 0.0f, 0.1f);
	}

	if (spatial_samples_changed && num_spatial_samples > 0) {
		vkDeviceWaitIdle(vk::context().device);
		prm::remove(reconnection_buffer);
		reconnection_buffer = prm::get_buffer(
			{.name = "Reservoir Connection",
			 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
					  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			 .memory_type = vk::BufferType::GPU,
			 .size = Window::width() * Window::height() * sizeof(ReconnectionData) * (num_spatial_samples + 1)});
	}
	return result;
}
