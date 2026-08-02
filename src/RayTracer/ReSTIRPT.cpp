#include "Integrator.h"
#include "Framework/VkUtils.h"
#include "ReSTIRPT.h"

using namespace RestirPT;
// TODO: Make sure that we handle the correct PDF computation in many light sampling (multiple lights with multiple
// emissives)

namespace restirpt {

void init(Integrator* integrator) {
	ReSTIRPT& state = integrator->restirpt;

	if (state.photon_bvh_scratch_bufs.size == 0) {
		state.photon_bvh_scratch_bufs.resize(vk::MAX_FRAMES_IN_FLIGHT);
		for (u64 i = 0; i < state.photon_bvh_scratch_bufs.size; i++) {
			state.photon_bvh_scratch_bufs[i] = nullptr;
		}
	}

	state.gris_gbuffer =
		prm::get_buffer({.name = CSTR("GRIS GBuffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(SurfaceRef)});

	state.gris_prev_gbuffer =
		prm::get_buffer({.name = CSTR("GRIS Previous GBuffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(SurfaceRef)});

	state.direct_lighting_texture = prm::get_texture({.name = CSTR("Direct Lighting Texture"),
												.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
												.dimensions = {Window::width(), Window::height(), 1},
												.format = VK_FORMAT_R32G32B32A32_SFLOAT});

	state.caustics_texture = prm::get_texture({.name = CSTR("Caustics Texture"),
										 .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
										 .dimensions = {Window::width(), Window::height(), 1},
										 .format = VK_FORMAT_R32G32B32A32_SFLOAT});
	state.gris_reservoir_ping_buffer =
		prm::get_buffer({.name = CSTR("GRIS Reservoirs Ping"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(RestirPT::Reservoir)});

	state.gris_reservoir_pong_buffer =
		prm::get_buffer({.name = CSTR("GRIS Reservoirs Pong"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(RestirPT::Reservoir)});

	state.prefix_contribution_buffer =
		prm::get_buffer({.name = CSTR("Prefix Contributions"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(lm::vec3)});

	state.debug_vis_buffer =
		prm::get_buffer({.name = CSTR("Debug Vis"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(u32)});
	state.reconnection_buffer = prm::get_buffer(
		{.name = CSTR("Reservoir Connection"),
		 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		 .memory_type = vk::BUFFER_TYPE_GPU,
		 .size = Window::width() * Window::height() * sizeof(ReconnectionData) * (state.num_spatial_samples + 1)});

	state.photon_eye_buffer_ping =
		prm::get_buffer({.name = CSTR("Photon - Eye - Ping"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(PhotonData)});

	state.photon_eye_buffer_pong =
		prm::get_buffer({.name = CSTR("Photon - Eye - Pong"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(PhotonData)});

	state.caustic_photon_aabbs_buffer =
		prm::get_buffer({.name = CSTR("Caustic Photon AABBs"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
								  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(f32) * 6});
	state.caustic_photon_light_buffer =
		prm::get_buffer({.name = CSTR("Caustic Photon - Light"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(PhotonData)});
	state.photon_count_buffer =
		prm::get_buffer({.name = CSTR("Photon Counts"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = 4});

	state.caustics_reservoir_ping_buffer =
		prm::get_buffer({.name = CSTR("Caustics Reservoirs Ping"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(PhotonReservoir)});

	state.caustics_reservoir_pong_buffer =
		prm::get_buffer({.name = CSTR("Caustics Reservoirs Pong"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(PhotonReservoir)});

	SceneDesc desc = integrator::scene_desc_base(integrator);
	// ReSTIR PT (GRIS)
	SET_SCENE_BUFFER(desc, gris_reservoir, state.gris_reservoir_ping_buffer);
	SET_SCENE_BUFFER(desc, prefix_contributions, state.prefix_contribution_buffer);
	SET_SCENE_BUFFER(desc, debug_vis, state.debug_vis_buffer);
	SET_SCENE_BUFFER(desc, photon_eye, state.photon_eye_buffer_ping);
	SET_SCENE_BUFFER(desc, caustic_photon_aabbs, state.caustic_photon_aabbs_buffer);
	SET_SCENE_BUFFER(desc, caustic_photon_light, state.caustic_photon_light_buffer);
	SET_SCENE_BUFFER(desc, photon_count, state.photon_count_buffer);
	integrator::upload_scene_desc(integrator, desc);

	state.canonical_contributions_texture = prm::get_texture({
		.name = CSTR("Canonical Contributions Texture"),
		.usage = VK_IMAGE_USAGE_STORAGE_BIT,
		.dimensions = {Window::width(), Window::height(), 1},
		.format = VK_FORMAT_R16G16B16A16_SFLOAT,
		.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
	});

	// For Photon TLAS
	VkAccelerationStructureInstanceKHR tlas_instance;
	tlas_instance.instanceCustomIndex = 0;
	tlas_instance.transform = vk::to_vk_matrix(lm::mat4(1.0f));
	tlas_instance.mask = 0xFF;
	tlas_instance.instanceShaderBindingTableRecordOffset = 0;
	tlas_instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
	// Set to 0, will be updated during BLAS build
	tlas_instance.accelerationStructureReference = 0;

	state.photon_bvh_instances_buf =
		prm::get_buffer({.name = CSTR("Photon BVH Instance"),
						 .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
						 .memory_type = vk::BUFFER_TYPE_CPU_TO_GPU,
						 .size = sizeof(VkAccelerationStructureInstanceKHR),
						 .create_mapped = true,
						 .data = &tlas_instance,
						 .dedicated_allocation = true});

	integrator->frame_num = 0;

	state.pc.total_frame_num = 0;
	state.pc.buffer_idx = 0;

	assert(rg::settings().shader_inference == true);

	state.path_length = integrator->lumen_scene->config.common.path_length;
}

void render(Integrator* integrator) {
	ReSTIRPT& state = integrator->restirpt;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.enable_temporal_jitter = uint(state.enable_temporal_jitter);
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.prev_random_num = state.pc.general_seed;
	state.pc.sampling_seed = lm::rand_u32();
	state.pc.seed2 = lm::rand_u32();
	state.pc.seed3 = lm::rand_u32();
	state.pc.max_depth = state.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.dir_light_idx = integrator->lumen_scene->dir_light_idx;
	state.pc.enable_accumulation = state.enable_accumulation;
	state.pc.num_spatial_samples = state.num_spatial_samples;
	state.pc.scene_extent = lm::length(integrator->lumen_scene->dimensions.max - integrator->lumen_scene->dimensions.min);
	state.pc.direct_lighting = state.direct_lighting;
	state.pc.enable_rr = state.enable_rr;

	state.pc.spatial_radius = state.spatial_reuse_radius;
	state.pc.enable_spatial_reuse = state.enable_spatial_reuse;
	state.pc.hide_reconnection_radiance = state.hide_reconnection_radiance;
	state.pc.min_vertex_distance_ratio = state.min_vertex_distance_ratio;
	state.pc.enable_gris = state.enable_gris;
	state.pc.frame_num = integrator->frame_num;
	state.pc.pixel_debug = state.pixel_debug;
	state.pc.temporal_reuse = uint(state.enable_temporal_reuse);
	state.pc.permutation_sampling = uint(state.enable_permutation_sampling);
	state.pc.gris_separator = state.gris_separator;
	state.pc.canonical_only = state.canonical_only;
	state.pc.enable_occlusion = state.enable_occlusion;
	state.pc.num_photons = state.num_photons;
	state.pc.photon_radius = state.initial_photon_radius;
	state.pc.pm_temporal_reuse = uint(state.enable_pm_temporal_reuse);

	if (state.progressive_radius_reduction) {
		state.pc.photon_radius = state.curr_photon_radius * sqrtf(((f32)integrator->frame_num + 2.0f / 3.0f) / ((f32)integrator->frame_num + 1.0f));
		state.curr_photon_radius = state.pc.photon_radius;
	}

	const std::initializer_list<lm::ResourceBinding> common_bindings = {
		integrator->output_tex, integrator->scene_ubo_buffer, integrator->lumen_scene->scene_desc_buffer, integrator->lumen_scene->mesh_lights_buffer};

	vk::Buffer* reservoir_buffers[] = {state.gris_reservoir_ping_buffer, state.gris_reservoir_pong_buffer};
	vk::Buffer* photon_reservoir_buffers[] = {state.caustics_reservoir_ping_buffer, state.caustics_reservoir_pong_buffer};
	vk::Buffer* photon_gbuffers[] = {state.photon_eye_buffer_ping, state.photon_eye_buffer_pong};
	vk::Buffer* gbuffers[] = {state.gris_prev_gbuffer, state.gris_gbuffer};

	i32 ping = state.pc.total_frame_num % 2;
	i32 pong = ping ^ 1;
	u64 resource_idx = vk::context().in_flight_frame_idx;

	constexpr i32 WRITE_OR_CURR_IDX = 1;
	constexpr i32 READ_OR_PREV_IDX = 0;
	if (state.enable_photon_mapping) {
		rg::add_rt(CSTR("PM - Trace First Diffuse"),
					 {
						 .shaders = {{CSTR("src/shaders/integrators/restir/gris/pm_trace_eye.rgen")},
									 {CSTR("src/shaders/surface.rmiss")},
									 {CSTR("src/shaders/ray_shadow.rmiss")},
									 {CSTR("src/shaders/surface.rchit")},
									 {CSTR("src/shaders/ray.rahit")}},
						 .macros = {vk::ShaderMacro("ENABLE_ATMOSPHERE", state.enable_atmosphere)},
						 .dims = {Window::width(), Window::height()},
					 })
			.push_constants(&state.pc)
			.bind(common_bindings)
			.bind(photon_gbuffers[pong])
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.bind_tlas(*integrator->tlas);

		// For BLAS
		VkAccelerationStructureGeometryAabbsDataKHR aabbs_data{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR};
		aabbs_data.data.deviceAddress = state.caustic_photon_aabbs_buffer->device_address();
		aabbs_data.stride = sizeof(f32) * 6;

		VkAccelerationStructureGeometryKHR as_geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		as_geom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
		as_geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		as_geom.geometry.aabbs = aabbs_data;

		VkAccelerationStructureBuildRangeInfoKHR offset;
		offset.firstVertex = 0;
		offset.primitiveCount = state.num_photons;
		offset.primitiveOffset = 0;
		offset.transformOffset = 0;

		state.photon_blas_input = {.geometry = as_geom, .build_range = offset};

		rg::add_rt(CSTR("PM - Trace Photons"),
					 {
						 .shaders = {{CSTR("src/shaders/integrators/restir/gris/pm_trace_photons.rgen")},
									 {CSTR("src/shaders/surface.rmiss")},
									 {CSTR("src/shaders/ray_shadow.rmiss")},
									 {CSTR("src/shaders/surface.rchit")},
									 {CSTR("src/shaders/ray.rahit")}},
						 .macros = {vk::ShaderMacro("ENABLE_ATMOSPHERE", state.enable_atmosphere),
									vk::ShaderMacro("DISABLE_PM_MIS", !state.enable_pm_mis)},
						 .dims = {state.num_photons, 1},
					 })
			.push_constants(&state.pc)
			.bind(common_bindings)
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.zero(state.photon_count_buffer)
			.zero(state.caustic_photon_aabbs_buffer)
			.blas_build(util::Slice(&state.photon_blas, 1), util::Slice(&state.photon_blas_input, 1),
						VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
						util::Slice(&state.caustic_photon_aabbs_buffer, 1), &state.photon_bvh_scratch_bufs[resource_idx])
			.tlas_build(state.photon_tlas, state.photon_bvh_instances_buf, 1,
						VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
						&state.photon_bvh_scratch_bufs[resource_idx],
						/*build_tlas_after_blas=*/true)
			.bind_tlas(*integrator->tlas);

		if (state.photon_tlas.accel && state.enable_photon_gather) {
			rg::add_rt(CSTR("Collect Photons"),
						 {
							 .shaders = {{CSTR("src/shaders/integrators/restir/gris/pm_collect_photons.rgen")},
										 {CSTR("src/shaders/surface.rmiss")},
										 {CSTR("src/shaders/ray_shadow.rmiss")},
										 {CSTR("src/shaders/surface.rchit")},
										 {CSTR("src/shaders/ray.rahit")}},
							 .macros = {{"STREAMING_MODE", i32(state.streaming_method)},
										vk::ShaderMacro("ENABLE_ATMOSPHERE", state.enable_atmosphere)},
							 .dims = {Window::width(), Window::height()},
						 })
				.push_constants(&state.pc)
				.bind(common_bindings)
				.bind(state.canonical_contributions_texture)
				.bind(state.caustics_texture)
				.bind(photon_reservoir_buffers[ping])
				.bind(photon_reservoir_buffers[pong])
				.bind(photon_gbuffers[ping])
				.bind(photon_gbuffers[pong])
				.bind_texture_array(integrator->lumen_scene->scene_textures)
				.bind_tlas(*integrator->tlas)
				.bind_tlas(state.photon_tlas);
		}
	}

	// Trace rays
	rg::add_rt(CSTR("GRIS - Generate Samples"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/restir/gris/gris.rgen")},
								 {CSTR("src/shaders/surface.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/surface.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .macros = {vk::ShaderMacro("STREAMING_MODE", i32(state.streaming_method)),
								vk::ShaderMacro("ENABLE_ATMOSPHERE", state.enable_atmosphere),
								vk::ShaderMacro("DISABLE_PM_MIS", !state.enable_pm_mis),
								vk::ShaderMacro("ENABLE_PM", state.enable_photon_gather && state.enable_photon_mapping)},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.zero(state.debug_vis_buffer)
		.bind(common_bindings)
		.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
		.bind(gbuffers[pong])
		.bind(state.canonical_contributions_texture)
		.bind(state.direct_lighting_texture)
		.bind(state.caustics_texture)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	state.pc.general_seed = lm::rand_u32();
	if (state.enable_gris) {
		bool should_do_temporal = state.enable_temporal_reuse && state.pc.total_frame_num > 0;
		// Temporal Reuse
		rg::add_rt(CSTR("GRIS - Temporal Reuse"),
					 {
						 .shaders = {{CSTR("src/shaders/integrators/restir/gris/temporal_reuse.rgen")},
									 {CSTR("src/shaders/surface.rmiss")},
									 {CSTR("src/shaders/ray_shadow.rmiss")},
									 {CSTR("src/shaders/surface.rchit")},
									 {CSTR("src/shaders/ray.rahit")}},
						 .dims = {Window::width(), Window::height()},
					 })
			.push_constants(&state.pc)
			.bind(common_bindings)
			.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
			.bind(reservoir_buffers[READ_OR_PREV_IDX])
			.bind(gbuffers[pong])
			.bind(gbuffers[ping])
			.bind(state.canonical_contributions_texture)
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.bind_tlas(*integrator->tlas)
			.skip_execution(!should_do_temporal);
		state.pc.seed2 = lm::rand_u32();
		if (!state.canonical_only) {
			if (state.mis_method == ReSTIRPT::MIS_TALBOT) {
				rg::add_rt(CSTR("GRIS - Spatial Reuse - Talbot"),
							 {
								 .shaders = {{CSTR("src/shaders/integrators/restir/gris/spatial_reuse_talbot.rgen")},
											 {CSTR("src/shaders/surface.rmiss")},
											 {CSTR("src/shaders/ray_shadow.rmiss")},
											 {CSTR("src/shaders/surface.rchit")},
											 {CSTR("src/shaders/ray.rahit")}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&state.pc)
					.bind(common_bindings)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(reservoir_buffers[READ_OR_PREV_IDX])
					.bind(gbuffers[pong])
					.bind(state.canonical_contributions_texture)
					.bind(state.direct_lighting_texture)
					.bind_texture_array(integrator->lumen_scene->scene_textures)
					.bind_tlas(*integrator->tlas);
			} else {
				// Retrace
				rg::add_rt(CSTR("GRIS - Retrace Reservoirs"),
							 {
								 .shaders = {{CSTR("src/shaders/integrators/restir/gris/retrace_paths.rgen")},
											 {CSTR("src/shaders/surface.rmiss")},
											 {CSTR("src/shaders/ray_shadow.rmiss")},
											 {CSTR("src/shaders/surface.rchit")},
											 {CSTR("src/shaders/ray.rahit")}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&state.pc)
					.bind(common_bindings)
					.bind(state.reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(gbuffers[pong])
					.bind_texture_array(integrator->lumen_scene->scene_textures)
					.bind_tlas(*integrator->tlas);
				// Validate
				rg::add_rt(CSTR("GRIS - Validate Samples"),
							 {
								 .shaders = {{CSTR("src/shaders/integrators/restir/gris/validate_samples.rgen")},
											 {CSTR("src/shaders/surface.rmiss")},
											 {CSTR("src/shaders/ray_shadow.rmiss")},
											 {CSTR("src/shaders/surface.rchit")},
											 {CSTR("src/shaders/ray.rahit")}},
								 .dims = {Window::width(), Window::height()},
							 })
					.push_constants(&state.pc)
					.bind(common_bindings)
					.bind(state.reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(gbuffers[pong])
					.bind_texture_array(integrator->lumen_scene->scene_textures)
					.bind_tlas(*integrator->tlas);

				// Spatial Reuse
				rg::add_rt(
						CSTR("GRIS - Spatial Reuse"),
						{
							.shaders = {{CSTR("src/shaders/integrators/restir/gris/spatial_reuse.rgen")},
										{CSTR("src/shaders/surface.rmiss")},
										{CSTR("src/shaders/ray_shadow.rmiss")},
										{CSTR("src/shaders/surface.rchit")},
										{CSTR("src/shaders/ray.rahit")}},
							.macros = {vk::ShaderMacro("ENABLE_DEFENSIVE_PAIRWISE_MIS", state.enable_defensive_formulation)},
							.dims = {Window::width(), Window::height()},
						})
					.push_constants(&state.pc)
					.bind(common_bindings)
					.bind(state.reconnection_buffer)
					.bind(reservoir_buffers[WRITE_OR_CURR_IDX])
					.bind(reservoir_buffers[READ_OR_PREV_IDX])
					.bind(gbuffers[pong])
					.bind(state.canonical_contributions_texture)
					.bind(state.direct_lighting_texture)
					.bind_texture_array(integrator->lumen_scene->scene_textures)
					.bind_tlas(*integrator->tlas);
			}
			if (state.pixel_debug || (state.gris_separator < 1.0f && state.gris_separator > 0.0f)) {
				u32 num_wgs = u32((Window::width() * Window::height() + 1023) / 1024);
				rg::add_compute(CSTR("GRIS - Debug Visualiation"),
								  {.shader = vk::Shader(CSTR("src/shaders/integrators/restir/gris/debug_vis.comp")),
								   .dims = {num_wgs}})
					.push_constants(&state.pc)
					.bind({integrator->output_tex, integrator->scene_ubo_buffer, integrator->lumen_scene->scene_desc_buffer});
			}
		}
	}

	state.pc.total_frame_num++;
}

bool update(Integrator* integrator) {
	ReSTIRPT& state = integrator->restirpt;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
		state.curr_photon_radius = state.initial_photon_radius;
	}
	return updated;
}

void destroy(Integrator* integrator, bool resize) {
	ReSTIRPT& state = integrator->restirpt;

	vk::Buffer** buffers[] = {&state.gris_gbuffer,
							 &state.gris_reservoir_ping_buffer,
							 &state.gris_reservoir_pong_buffer,
							 &state.prefix_contribution_buffer,
							 &state.reconnection_buffer,
							 &state.gris_prev_gbuffer,
							 &state.debug_vis_buffer,
							 &state.photon_eye_buffer_ping,
							 &state.photon_eye_buffer_pong,
							 &state.caustic_photon_aabbs_buffer,
							 &state.caustic_photon_light_buffer,
							 &state.photon_count_buffer,
							 &state.caustics_reservoir_ping_buffer,
							 &state.caustics_reservoir_pong_buffer,
							 &state.photon_bvh_instances_buf};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}

	vk::Texture** textures[] = {&state.canonical_contributions_texture, &state.direct_lighting_texture,
							   &state.caustics_texture};
	for (vk::Texture** texture : textures) {
		prm::remove(*texture);
		*texture = nullptr;
	}

	if (!resize) {
		vkDeviceWaitIdle(vk::context().device);
		state.photon_tlas.destroy();
		state.photon_blas.destroy();
		for (vk::Buffer*& scratch_buf : state.photon_bvh_scratch_bufs) {
			drm::destroy(scratch_buf);
			scratch_buf = nullptr;
		}
	}
}

bool gui(Integrator* integrator) {
	ReSTIRPT& state = integrator->restirpt;
	bool result = false;
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	result |= ImGui::Checkbox("Direct lighting", &state.direct_lighting);
	result |= ImGui::Checkbox("Enable atmosphere", &state.enable_atmosphere);
	result |= ImGui::Checkbox("Enable Russian roulette", &state.enable_rr);
	result |= ImGui::Checkbox("Enable GRIS", &state.enable_gris);
	result |= ImGui::SliderInt("Path length", (i32*)&state.path_length, 1, 12);
	result |= ImGui::Checkbox("Enable canonical-only mode", &state.canonical_only);

	result |= ImGui::Checkbox("Enable photon mapping", &state.enable_photon_mapping);
	if (state.enable_photon_mapping) {
		bool num_photons_changed =
			ImGui::SliderInt("Num photons", (i32*)&state.num_photons, 1, Window::width() * Window::height());
		result |= num_photons_changed;
		result |= ImGui::SliderFloat("Initial photon radius", &state.initial_photon_radius, 0.0f, 0.1f);
		result |= ImGui::Checkbox("Progressive radius reduction", &state.progressive_radius_reduction);
		ImGui::Text("Current photon radius: %f", state.curr_photon_radius);
		result |= ImGui::Checkbox("Enable photon gather", &state.enable_photon_gather);
		result |= ImGui::Checkbox("MIS between NEE/BRDF/PM", &state.enable_pm_mis);
		result |= ImGui::Checkbox("Enable PM temporal reuse", &state.enable_pm_temporal_reuse);
		if (num_photons_changed) {
			vkDeviceWaitIdle(vk::context().device);

			for (u64 i = 0; i < vk::MAX_FRAMES_IN_FLIGHT; i++) {
				state.photon_blas.destroy();
				state.photon_tlas.destroy();
			}
		}
	}
	if (!state.enable_gris) {
		return result;
	}
	i32 curr_streaming_method = static_cast<i32>(state.streaming_method);
	const char* streaming_methods[] = {
		"Individual contributions",
		"Split at reconnection",
	};
	if (ImGui::Combo("Streaming method", &curr_streaming_method, streaming_methods, ARRAY_LEN(streaming_methods))) {
		result = true;
		state.streaming_method = static_cast<ReSTIRPT::StreamingMethod>(curr_streaming_method);
	}
	if (state.canonical_only) {
		return result;
	}
	result |= ImGui::SliderFloat("GRIS / Default", &state.gris_separator, 0.0f, 1.0f);
	result |= ImGui::Checkbox("Enable occlusion", &state.enable_occlusion);
	result |= ImGui::Checkbox("Temporal jitter", &state.enable_temporal_jitter);
	result |= ImGui::Checkbox("Debug pixels", &state.pixel_debug);
	result |= ImGui::Checkbox("Enable defensive formulation", &state.enable_defensive_formulation);
	result |= ImGui::Checkbox("Enable permutation sampling", &state.enable_permutation_sampling);
	result |= ImGui::Checkbox("Enable spatial reuse", &state.enable_spatial_reuse);
	result |= ImGui::Checkbox("Hide reconnection radiance", &state.hide_reconnection_radiance);
	const char* mis_methods[] = {
		"Talbot (Reconnection only)",
		"Pairwise",
	};
	i32 curr_mis_method = static_cast<i32>(state.mis_method);
	if (ImGui::Combo("MIS method", &curr_mis_method, mis_methods, ARRAY_LEN(mis_methods))) {
		result = true;
		state.mis_method = static_cast<ReSTIRPT::MISMethod>(curr_mis_method);
	}
	result |= ImGui::Checkbox("Enable temporal reuse", &state.enable_temporal_reuse);
	bool spatial_samples_changed = ImGui::SliderInt("Num spatial samples", (i32*)&state.num_spatial_samples, 0, 12);
	result |= spatial_samples_changed;
	result |= ImGui::SliderFloat("Spatial radius", &state.spatial_reuse_radius, 0.0f, 128.0f);
	result |= ImGui::SliderFloat("Min reconnection distance ratio", &state.min_vertex_distance_ratio, 0.0f, 1.0f);

	if (spatial_samples_changed && state.num_spatial_samples > 0) {
		vkDeviceWaitIdle(vk::context().device);
		prm::remove(state.reconnection_buffer);
		state.reconnection_buffer = prm::get_buffer(
			{.name = CSTR("Reservoir Connection"),
			 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
					  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			 .memory_type = vk::BUFFER_TYPE_GPU,
			 .size = Window::width() * Window::height() * sizeof(ReconnectionData) * (state.num_spatial_samples + 1)});
	}
	if (result) {
		state.pc.total_frame_num = 0;
	}
	return result;
}

}  // namespace restirpt
