#include "LumenPCH.h"
#include "Integrator.h"
#include "shaders/commons.h"
#include <Framework/Window.h>
#include <stb_image/stb_image.h>
#include "Framework/VkUtils.h"

void Integrator::init() {
	lumen::LumenInstance* instance = this->instance;
	Window* window = instance->window;

	lumen::Camera* cam_ptr = lumen_scene->camera.get();
	instance->window->add_mouse_click_callback(
		[cam_ptr, this, window](MouseAction button, KeyAction action, double x, double y) {
			if (ImGui::GetIO().WantCaptureMouse) {
				return;
			}
			if (updated && window->is_mouse_up(MouseAction::LEFT)) {
				updated = true;
			}
			if (updated && window->is_mouse_down(MouseAction::LEFT)) {
				updated = true;
			}
		});
	instance->window->add_mouse_move_callback(
		[window, cam_ptr, this](double delta_x, double delta_y, double x, double y) {
			if (ImGui::GetIO().WantCaptureMouse) {
				return;
			}
			if (window->is_mouse_held(MouseAction::LEFT) && !window->is_key_held(KeyInput::KEY_TAB)) {
				cam_ptr->rotate(0.05f * (float)delta_y, -0.05f * (float)delta_x, 0.0f);
				updated = true;
			}
		});

	// Create offscreen image for output
	lumen::TextureSettings settings;
	settings.usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
						   VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	settings.base_extent = {(uint32_t)instance->width, (uint32_t)instance->height, 1};
	settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	output_tex.create_empty_texture("Color Output", settings, VK_IMAGE_LAYOUT_GENERAL);

	scene_ubo_buffer.create("Scene UBO", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							sizeof(SceneUBO));

	update_uniform_buffers();
	create_accel();
}

bool Integrator::gui() {
	ImGui::NewLine();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	ImGui::Text("Integrator settings:");
	ImGui::PopStyleColor();
	return false;
}

void Integrator::update_uniform_buffers() {
	lumen_scene->camera->update_view_matrix();
	scene_ubo.prev_view = scene_ubo.view;
	scene_ubo.view = lumen_scene->camera->view;
	scene_ubo.prev_projection = lumen_scene->camera->projection;
	scene_ubo.projection = lumen_scene->camera->projection;
	scene_ubo.view_pos = glm::vec4(lumen_scene->camera->position, 1);
	scene_ubo.inv_view = glm::inverse(lumen_scene->camera->view);
	scene_ubo.inv_projection = glm::inverse(lumen_scene->camera->projection);
	scene_ubo.model = glm::mat4(1.0);
	scene_ubo.light_pos = glm::vec4(3.0f, 2.5f, 1.0f, 1.0f);
	memcpy(scene_ubo_buffer.data, &scene_ubo, sizeof(scene_ubo));
}

bool Integrator::update() {
	float trans_speed = 0.01f;
	glm::vec3 front;
	if (instance->window->is_key_held(KeyInput::KEY_LEFT_SHIFT)) {
		trans_speed *= 4;
	}

	front.x = cos(glm::radians(lumen_scene->camera->rotation.x)) * sin(glm::radians(lumen_scene->camera->rotation.y));
	front.y = sin(glm::radians(lumen_scene->camera->rotation.x));
	front.z = cos(glm::radians(lumen_scene->camera->rotation.x)) * cos(glm::radians(lumen_scene->camera->rotation.y));
	front = glm::normalize(-front);
	if (instance->window->is_key_held(KeyInput::KEY_W)) {
		lumen_scene->camera->position += front * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_A)) {
		lumen_scene->camera->position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_S)) {
		lumen_scene->camera->position -= front * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_D)) {
		lumen_scene->camera->position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::SPACE) || instance->window->is_key_held(KeyInput::KEY_E)) {
		// Right
		auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		lumen_scene->camera->position += up * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_LEFT_CONTROL) || instance->window->is_key_held(KeyInput::KEY_Q)) {
		auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		lumen_scene->camera->position -= up * trans_speed;
		updated = true;
	}

	if (instance->window->is_mouse_held(MouseAction::LEFT, scene_ubo.clicked_pos) &&
		instance->window->is_key_held(KeyInput::KEY_TAB)) {
		scene_ubo.debug_click = 1;
	} else {
		scene_ubo.debug_click = 0;
	}

	bool result = false;
	if (updated) {
		result = true;
		updated = false;
	}
	update_uniform_buffers();

	return result;
}
void Integrator::create_accel() {
	std::vector<vk::BlasInput> blas_inputs;

	auto vertex_address = lumen_scene->vertex_buffer.get_device_address();
	auto idx_address = lumen_scene->index_buffer.get_device_address();
	for (auto& prim_mesh : lumen_scene->prim_meshes) {
		vk::BlasInput geo = vk::to_vk_geometry(prim_mesh, vertex_address, idx_address);
		blas_inputs.push_back({geo});
	}
	vk::build_blas(blases, blas_inputs, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

	const auto& indices = lumen_scene->indices;
	const auto& vertices = lumen_scene->positions;
	std::vector<VkAccelerationStructureInstanceKHR> tlas_instances;
	for (const auto& pm : lumen_scene->prim_meshes) {
		VkAccelerationStructureInstanceKHR ray_inst{};
		ray_inst.transform = vk::to_vk_matrix(pm.world_matrix);
		ray_inst.instanceCustomIndex = pm.prim_idx;
		assert(pm.prim_idx < blases.size());
		ray_inst.accelerationStructureReference = blases[pm.prim_idx].get_blas_device_address();
		ray_inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		ray_inst.mask = 0xFF;
		ray_inst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
		tlas_instances.emplace_back(ray_inst);
	}
	vk::build_tlas(tlas, tlas_instances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void Integrator::destroy() {
	std::vector<lumen::BufferOld*> buffer_list = {&scene_ubo_buffer, &lumen_scene->scene_desc_buffer};

	for (auto b : buffer_list) {
		b->destroy();
	}
	output_tex.destroy();

	if (tlas.accel) {
		tlas.buffer.destroy();
		vkDestroyAccelerationStructureKHR(vk::context().device, tlas.accel, nullptr);
	}
	if (!blases.empty()) {
		for (auto& b : blases) {
			b.buffer.destroy();
			vkDestroyAccelerationStructureKHR(vk::context().device, b.accel, nullptr);
		}
	}
	blases.clear();
}
