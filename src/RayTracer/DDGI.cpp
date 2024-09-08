#include "../LumenPCH.h"
#include "DDGI.h"
#include <random>
constexpr int IRRADIANCE_SIDE_LENGTH = 8;
constexpr int DEPTH_SIDE_LENGTH = 16;

void DDGI::init() {
	Integrator::init();
	uint32_t num_probes;
	// DDGI Resources
	{
		glm::vec3 min_pos = lumen_scene->m_dimensions.min - vec3(0.1f);
		glm::vec3 max_pos = lumen_scene->m_dimensions.max + vec3(0.1f);
		glm::vec3 diag = (max_pos - min_pos) * 1.1f;
		probe_counts = glm::ivec3(diag / probe_distance);
		probe_start_position = min_pos;
		glm::vec3 bbox_div_probes = diag / glm::vec3(probe_counts);
		max_distance = bbox_div_probes.length() * 1.5f;

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
			vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &bilinear_sampler),
					  "Could not create image sampler");
			sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			sampler_ci.minFilter = VK_FILTER_NEAREST;
			sampler_ci.magFilter = VK_FILTER_NEAREST;
			vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &nearest_sampler),
					  "Could not create image sampler");
		}

		const uint32_t irradiance_width = (IRRADIANCE_SIDE_LENGTH + 2) * probe_counts.x * probe_counts.y;
		const uint32_t irradiance_height = (IRRADIANCE_SIDE_LENGTH + 2) * probe_counts.z;
		const uint32_t depth_width = (DEPTH_SIDE_LENGTH + 2) * probe_counts.x * probe_counts.y;
		const uint32_t depth_height = (DEPTH_SIDE_LENGTH + 2) * probe_counts.z;
		// Irradiance and depth
		num_probes = probe_counts.x * probe_counts.y * probe_counts.z;
		for (int i = 0; i < 2; i++) {
			irr_texes[i] = prm::get_texture({
				.name = "DDGI Irradiance " + std::to_string(i),
				.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.dimensions = {irradiance_width, irradiance_height, 1},
				.format = VK_FORMAT_R16G16B16A16_SFLOAT,
				.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
				.sampler = bilinear_sampler,
			});

			depth_texes[i] = prm::get_texture({
				.name = "DDGI Depth " + std::to_string(i),
				.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.dimensions = {depth_width, depth_height, 1},
				.format = VK_FORMAT_R16G16_SFLOAT,
				.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
				.sampler = bilinear_sampler,
			});
		}
		// RT
		create_radiance_textures();
		// DDGI Output
		output.tex = prm::get_texture({
			.name = "DDGI Output",
			.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.dimensions = {Window::width(), Window::height(), 1},
			.format = VK_FORMAT_R16G16B16A16_SFLOAT,
			.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
			.sampler = bilinear_sampler,
		});
	}
	g_buffer = prm::get_buffer({
		.name = "GBuffer",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = Window::width() * Window::height() * sizeof(GBufferData),
	});

	direct_lighting_buffer = prm::get_buffer({
		.name = "Direct Lighting",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = Window::width() * Window::height() * sizeof(glm::vec3),
	});

	ddgi_ubo_buffer = prm::get_buffer({
		.name = "DDGI UBO",
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.memory_type = vk::BufferType::CPU_TO_GPU,
		.size = sizeof(DDGIUniforms),
	});

	probe_offsets_buffer = prm::get_buffer({
		.name = "Probe Offsets",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = sizeof(vec4) * num_probes,
	});

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->get_device_address();

	desc.material_addr = lumen_scene->materials_buffer->get_device_address();
	// DDGI
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	desc.direct_lighting_addr = direct_lighting_buffer->get_device_address();
	desc.probe_offsets_addr = probe_offsets_buffer->get_device_address();
	desc.g_buffer_addr = g_buffer->get_device_address();

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, direct_lighting_addr, direct_lighting_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, probe_offsets_addr, probe_offsets_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, g_buffer_addr, g_buffer, vk::render_graph());

	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	update_ddgi_uniforms();
	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
}

void DDGI::render() {
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.first_frame = first_frame;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.frame_num = frame_num;
	bool ping_pong = bool(frame_idx % 2);  // ping_pong true = read
	// Generate random orientation for probes
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> dis(-1.0, 1.0);
		glm::vec4 rands(0.5 * dis(gen) + 0.5, dis(gen), dis(gen), dis(gen));
		pc_ray.probe_rotation = glm::mat4_cast(
			glm::angleAxis(2.0f * glm::pi<float>() * rands.x, glm::normalize(glm::vec3(rands.y, rands.z, rands.w))));
	}
	const std::initializer_list<lumen::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};
	// Trace Primary rays and fill G buffer
	vk::render_graph()
		->add_rt("DDGI - GBuffer Pass",
				 {
					 .shaders = {{"src/shaders/integrators/ddgi/primary_rays.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .specialization_data = {1},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&pc_ray)
		.zero(g_buffer)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	// Trace rays from probes
	uint32_t grid_size = probe_counts.x * probe_counts.y * probe_counts.z;
	vk::render_graph()
		->add_rt("DDGI - Probe Trace",
				 {
					 .shaders = {{"src/shaders/integrators/ddgi/trace.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .specialization_data = {1},
					 .dims = {(uint32_t)rays_per_probe, grid_size},
				 })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind({lumen_scene->mesh_lights_buffer, ddgi_ubo_buffer, rt.radiance_tex, rt.dir_depth_tex})
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	// Classify
	uint32_t wg_x = (probe_counts.x * probe_counts.y * probe_counts.z + 31) / 32;
	vk::render_graph()
		->add_compute("Classify Probes",
					  {.shader = vk::Shader("src/shaders/integrators/ddgi/classify.comp"), .dims = {wg_x}})
		.push_constants(&pc_ray)
		.bind({scene_ubo_buffer, lumen_scene->scene_desc_buffer, ddgi_ubo_buffer, rt.radiance_tex, rt.dir_depth_tex});
	// Update probes & borders
	{
		// Probes
		uint32_t wg_x = probe_counts.x * probe_counts.y;
		uint32_t wg_y = probe_counts.z;
		auto update_probe = [&](bool is_irr) {
			vk::render_graph()
				->add_compute(
					is_irr ? "Update Irradiance" : "Update Depth",
					{.shader = vk::Shader("src/shaders/integrators/ddgi/update.comp"),
					 .macros = {is_irr ? vk::ShaderMacro("IRRADIANCE_UPDATE") : vk::ShaderMacro("DEPTH_UPDATE")},
					 .dims = {wg_x, wg_y}})
				.push_constants(&pc_ray)
				.bind({lumen_scene->scene_desc_buffer, irr_texes[!ping_pong], depth_texes[!ping_pong],
					   irr_texes[ping_pong], depth_texes[ping_pong], ddgi_ubo_buffer, rt.radiance_tex,
					   rt.dir_depth_tex});
		};
		update_probe(true);
		update_probe(false);
		// Borders
		// 13 WGs process 4 probes (wg = 32 threads)
		wg_x = (probe_counts.x * probe_counts.y * probe_counts.z + 3) * 13 / 4;
		vk::render_graph()
			->add_compute("Update Borders",
						  {.shader = vk::Shader("src/shaders/integrators/ddgi/update_borders.comp"), .dims = {wg_x}})
			.push_constants(&pc_ray)
			.bind({irr_texes[!ping_pong], depth_texes[!ping_pong], ddgi_ubo_buffer});
	}
	// Sample probes & output into texture
	wg_x = (Window::width() + 31) / 32;
	uint32_t wg_y = (Window::height() + 31) / 32;
	vk::render_graph()
		->add_compute("Sample Probes",
					  {.shader = vk::Shader("src/shaders/integrators/ddgi/sample.comp"), .dims = {wg_x, wg_y}})
		.push_constants(&pc_ray)
		.bind({scene_ubo_buffer, lumen_scene->scene_desc_buffer, output.tex, irr_texes[ping_pong],
			   depth_texes[ping_pong], ddgi_ubo_buffer});
	// Relocate
	if (total_frame_idx < 5) {
		// 13 WGs process 4 probes (wg = 32 threads)
		wg_x = (probe_counts.x * probe_counts.y * probe_counts.z + 31) / 32;
		vk::render_graph()
			->add_compute("Relocate",
						  {.shader = vk::Shader("src/shaders/integrators/ddgi/relocate.comp"), .dims = {wg_x}})
			.push_constants(&pc_ray)
			.bind({scene_ubo_buffer, lumen_scene->scene_desc_buffer, ddgi_ubo_buffer, rt.dir_depth_tex});
	}
	// Output
	wg_x = (Window::width() + 31) / 32;
	wg_y = (Window::height() + 31) / 32;
	vk::render_graph()
		->add_compute("DDGI Output",
					  {.shader = vk::Shader("src/shaders/integrators/ddgi/out.comp"), .dims = {wg_x, wg_y}})
		.push_constants(&pc_ray)
		.bind({output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer, output.tex});
	frame_idx++;
	total_frame_idx++;
	first_frame = false;
}

bool DDGI::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	update_ddgi_uniforms();
	return updated;
}
bool DDGI::gui() {
	bool result = Integrator::gui();
	result |= ImGui::SliderFloat("Hysteresis", &hysteresis, 0.0f, 1.0f);
	bool rpp_changed = ImGui::SliderInt("Rays per probe", (int*)&rays_per_probe, 1, 4096);
	if(rpp_changed && (rays_per_probe > 0)) {
		vkDeviceWaitIdle(vk::context().device);
		prm::remove(rt.radiance_tex);
		prm::remove(rt.dir_depth_tex);
		create_radiance_textures();
	}
	result |= rpp_changed;
	result |= ImGui::SliderFloat("Ray max distance", &tmax, 0.0f, 1000.0f);
	result |= ImGui::SliderFloat("Ray min distance", &tmin, 0.0f, 1000.0f);
	ImGui::Text("Probe dimensions: %dx%dx%d", probe_counts.x, probe_counts.y, probe_counts.z);
	ImGui::Text("Probe distance: %f", probe_distance);
	const uint32_t num_rays = rays_per_probe * probe_counts.x * probe_counts.y * probe_counts.z;
	ImGui::Text("Number of rays: %d", num_rays);
	ImGui::Text("Rays per pixel: %f", (float)num_rays / (Window::width() * Window::height()));
	return result;
}

void DDGI::update_ddgi_uniforms() {
	ddgi_ubo.probe_counts = probe_counts;
	ddgi_ubo.hysteresis = hysteresis;
	ddgi_ubo.probe_start_position = probe_start_position;
	ddgi_ubo.probe_step = probe_distance;
	ddgi_ubo.rays_per_probe = rays_per_probe;
	ddgi_ubo.max_distance = max_distance;
	ddgi_ubo.depth_sharpness = depth_sharpness;
	ddgi_ubo.normal_bias = normal_bias;
	ddgi_ubo.view_bias = view_bias;
	ddgi_ubo.irradiance_width = irr_texes[0]->extent.width;
	ddgi_ubo.irradiance_height = irr_texes[0]->extent.height;
	ddgi_ubo.depth_width = depth_texes[0]->extent.width;
	ddgi_ubo.depth_height = depth_texes[0]->extent.height;
	ddgi_ubo.backface_ratio = backface_ratio;
	ddgi_ubo.min_frontface_dist = min_frontface_dist;
	ddgi_ubo.tmax = tmax;
	ddgi_ubo.tmin = tmin;
	vk::write_buffer(ddgi_ubo_buffer, &ddgi_ubo, sizeof(ddgi_ubo));
}

void DDGI::create_radiance_textures() { 
	uint32_t num_probes = probe_counts.x * probe_counts.y * probe_counts.z;
	rt.radiance_tex = prm::get_texture({
				.name = "DDGI Radiance",
				.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.dimensions = {rays_per_probe, num_probes, 1},
				.format = VK_FORMAT_R16G16B16A16_SFLOAT,
				.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
				.sampler = nearest_sampler,
			});
			rt.dir_depth_tex = prm::get_texture({
				.name = "DDGI Radiance & Tex",
				.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.dimensions = {rays_per_probe, num_probes, 1},
				.format = VK_FORMAT_R16G16B16A16_SFLOAT,
				.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
				.sampler = nearest_sampler,
			});
}

void DDGI::destroy() {
	Integrator::destroy();
	auto buffer_list = {g_buffer, direct_lighting_buffer, ddgi_ubo_buffer, probe_offsets_buffer};

	auto tex_list = {rt.radiance_tex, rt.dir_depth_tex, output.tex};

	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}

	for (auto t : tex_list) {
		prm::remove(t);
	}

	vkDestroySampler(vk::context().device, bilinear_sampler, nullptr);
	vkDestroySampler(vk::context().device, nearest_sampler, nullptr);

	for (vk::Texture* irr_tex : irr_texes) {
		prm::remove(irr_tex);
	}

	for (vk::Texture* depth_tex : depth_texes) {
		prm::remove(depth_tex);
	}
}
