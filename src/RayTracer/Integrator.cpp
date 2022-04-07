#include "LumenPCH.h"
#include "Integrator.h"
#include <stb_image.h>

void Integrator::init() {
	VkPhysicalDeviceProperties2 prop2{
	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	prop2.pNext = &rt_props;
	vkGetPhysicalDeviceProperties2(instance->vkb.ctx.physical_device, &prop2);
	constexpr int VERTEX_BINDING_ID = 0;


	LumenInstance* instance = this->instance;
	Window* window = instance->window;
	lumen_scene.load_scene("scenes/", config.filename);
	if (lumen_scene.cam_config.pos != vec3(0)) {
		camera = std::unique_ptr<PerspectiveCamera>(new PerspectiveCamera(
			lumen_scene.cam_config.fov, 0.01f, 1000.0f, (float)instance->width / instance->height,
			lumen_scene.cam_config.dir, lumen_scene.cam_config.pos));
	} else {
		// Assume the camera matrix is given
		camera = std::unique_ptr<PerspectiveCamera>(new PerspectiveCamera(
			lumen_scene.cam_config.fov, lumen_scene.cam_config.cam_matrix, 0.01f, 1000.0f, 
			(float)instance->width / instance->height));
	}

	Camera* cam_ptr = camera.get();
	instance->window->add_mouse_click_callback(
		[cam_ptr, this, window](MouseAction button, KeyAction action) {
		if (updated && window->is_mouse_up(MouseAction::LEFT)) {
			updated = true;
		}
		if (updated && window->is_mouse_down(MouseAction::LEFT)) {
			updated = true;
		}
	});
	instance->window->add_mouse_move_callback(
		[window, cam_ptr, this](double delta_x, double delta_y) {
		if (window->is_mouse_held(MouseAction::LEFT)) {
			cam_ptr->rotate(0.05f * (float)delta_y, -0.05f * (float)delta_x,
							0.0f);
			//pc_ray.frame_num = -1;
			updated = true;
		}
	});
	auto vertex_buf_size = lumen_scene.positions.size() * sizeof(glm::vec3);
	auto idx_buf_size = lumen_scene.indices.size() * sizeof(uint32_t);
	std::vector<PrimMeshInfo> prim_lookup;
	uint32_t idx = 0;
	for (auto& pm : lumen_scene.prim_meshes) {
		PrimMeshInfo m_info;
		m_info.index_offset = pm.first_idx;
		m_info.vertex_offset = pm.vtx_offset;
		m_info.material_index = pm.material_idx;
		m_info.min_pos = glm::vec4(pm.min_pos, 0);
		m_info.max_pos = glm::vec4(pm.max_pos, 0);
		m_info.material_index = pm.material_idx;
		prim_lookup.emplace_back(m_info);
		auto& mef = lumen_scene.materials[pm.material_idx].emissive_factor;
		if (mef.x > 0 || mef.y > 0 || mef.z > 0) {
			Light light;
			light.num_triangles = pm.idx_count / 3;
			light.prim_mesh_idx = idx;
			light.light_flags = LIGHT_AREA;
			lights.emplace_back(light);
			total_light_triangle_cnt += light.num_triangles;
		}
		idx++;
	}

	for (auto& l : lumen_scene.lights) {
		Light light;
		light.L = l.L;
		light.light_flags = l.light_type;
		light.pos = l.pos;
		light.to = l.to;
		total_light_triangle_cnt++;
		light.world_radius = lumen_scene.m_dimensions.radius;
		light.world_center = 0.5f * (lumen_scene.m_dimensions.max + lumen_scene.m_dimensions.min);
		lights.emplace_back(light);
	}


	scene_ubo_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneUBO));
	update_uniform_buffers();

	vertex_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		vertex_buf_size, lumen_scene.positions.data(), true);
	index_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		idx_buf_size, lumen_scene.indices.data(), true);

	normal_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		lumen_scene.normals.size() * sizeof(lumen_scene.normals[0]),
		lumen_scene.normals.data(), true);
	uv_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		lumen_scene.texcoords0.size() * sizeof(glm::vec2),
		lumen_scene.texcoords0.data(), true);
	materials_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		lumen_scene.materials.size() * sizeof(Material), lumen_scene.materials.data(), true);
	prim_lookup_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		prim_lookup.size() * sizeof(PrimMeshInfo), prim_lookup.data(), true);

	// Create a sampler for textures
	VkSamplerCreateInfo sampler_ci = vk::sampler_create_info();
	sampler_ci.minFilter = VK_FILTER_LINEAR;
	sampler_ci.magFilter = VK_FILTER_LINEAR;
	sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_ci.maxLod = FLT_MAX;
	vk::check(vkCreateSampler(instance->vkb.ctx.device, &sampler_ci, nullptr,
			  &texture_sampler),
			  "Could not create image sampler");

	auto add_default_texture = [this, instance]() {
		std::array<uint8_t, 4> nil = { 0, 0, 0, 0 };
		textures.resize(1);
		auto ci = make_img2d_ci(VkExtent2D{ 1, 1 });
		textures[0].load_from_data(&instance->vkb.ctx, nil.data(), 4, ci,
								   texture_sampler);
	};

	if (!lumen_scene.textures.size()) {
		add_default_texture();
	} else {
		textures.resize(lumen_scene.textures.size());
		int i = 0;
		for (const auto& texture_path : lumen_scene.textures) {
			int x, y, n;
			unsigned char* data = stbi_load(texture_path.c_str(), &x, &y, &n, 4);
		
			auto size = x * y * 4 ;
			auto img_dims =
				VkExtent2D{ (uint32_t)x, (uint32_t)y };
			auto ci = make_img2d_ci(img_dims, VK_FORMAT_R8G8B8A8_SRGB,
									VK_IMAGE_USAGE_SAMPLED_BIT, false);
			textures[i].load_from_data(&instance->vkb.ctx, data, size, ci,
									   texture_sampler, false);
			stbi_image_free(data);
			i++;
		}
	}
}

void Integrator::update_uniform_buffers() {
	camera->update_view_matrix();
	scene_ubo.view = camera->view;
	scene_ubo.projection = camera->projection;
	scene_ubo.view_pos = glm::vec4(camera->position, 1);
	scene_ubo.inv_view = glm::inverse(camera->view);
	scene_ubo.inv_projection = glm::inverse(camera->projection);
	scene_ubo.model = glm::mat4(1.0);
	scene_ubo.light_pos = glm::vec4(3.0f, 2.5f, 1.0f, 1.0f);
	memcpy(scene_ubo_buffer.data, &scene_ubo, sizeof(scene_ubo));
}

void Integrator::destroy() {
	std::vector<Buffer*> buffer_list = {
	&vertex_buffer, &normal_buffer, &uv_buffer,
	&index_buffer, &materials_buffer, &prim_lookup_buffer,
	&scene_desc_buffer, &scene_ubo_buffer
	};
	if (lights.size()) {
		buffer_list.push_back(&mesh_lights_buffer);
	}
	for (auto b : buffer_list) {
		b->destroy();
	}
	output_tex.destroy();
	for (auto& tex : textures) {
		tex.destroy();
	}
	vkDestroySampler(instance->vkb.ctx.device, texture_sampler, nullptr);
}
