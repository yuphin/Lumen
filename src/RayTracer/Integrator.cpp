#include "Integrator.h"
#include "shaders/commons.h"
#include <Framework/Window.h>
#include "Framework/VkUtils.h"

namespace integrator {

static bool no_gui(Integrator*) { return false; }

static void update_uniform_buffers(Integrator* integrator) {
	lm::Camera& camera = integrator->lumen_scene->camera;
	lm::camera_update_view(&camera);
	integrator->scene_ubo.prev_view = integrator->scene_ubo.view;
	integrator->scene_ubo.view = camera.view;
	integrator->scene_ubo.prev_projection = integrator->scene_ubo.projection;
	integrator->scene_ubo.projection = camera.projection;
	integrator->scene_ubo.view_pos = lm::vec4(camera.position, 1);
	integrator->scene_ubo.inv_view = lm::inverse(camera.view);
	integrator->scene_ubo.inv_projection = lm::inverse(camera.projection);
	integrator->scene_ubo.model = lm::mat4(1.0);
	integrator->scene_ubo.light_pos = lm::vec4(3.0f, 2.5f, 1.0f, 1.0f);
	integrator->scene_ubo.cam_dir = integrator->scene_ubo.inv_view * lm::vec4(camera.direction, 0);
	integrator->scene_ubo.fovy = camera.fov;
}

static void upload_scene_ubo(Integrator* integrator) {
	integrator->scene_ubo_buffer = integrator->scene_ubo_buffers[vk::context().in_flight_frame_idx];
	vk::buffer_write(integrator->scene_ubo_buffer, &integrator->scene_ubo, sizeof(integrator->scene_ubo));
}

static void mouse_click_callback(void* user_data, MouseAction, KeyAction, double, double) {
	Integrator* integrator = (Integrator*)user_data;
	if (ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	if (integrator->updated && Window::is_mouse_up(MouseAction::LEFT)) {
		integrator->updated = true;
	}
	if (integrator->updated && Window::is_mouse_down(MouseAction::LEFT)) {
		integrator->updated = true;
	}
}

static void mouse_move_callback(void* user_data, double delta_x, double delta_y, double, double) {
	Integrator* integrator = (Integrator*)user_data;
	if (ImGui::GetIO().WantCaptureMouse) {
		return;
	}
	if (Window::is_mouse_held(MouseAction::LEFT) && !Window::is_key_held(KeyInput::KEY_TAB)) {
		lm::camera_rotate(&integrator->lumen_scene->camera, 0.05f * (f32)delta_y, -0.05f * (f32)delta_x, 0.0f);
		integrator->updated = true;
	}
}

static void common_init(Integrator* integrator) {
	if (!integrator->lumen_scene) {
		integrator->lumen_scene = scene::get();
	}
	if (!integrator->arena) {
		integrator->arena = lm::arena_create(CSTR("Integrator Arena"), MB(1), MB(1));
	}
	if (!integrator->callbacks_registered) {
		Window::add_mouse_click_callback(mouse_click_callback, integrator);
		Window::add_mouse_move_callback(mouse_move_callback, integrator);
		integrator->callbacks_registered = true;
	}

	integrator->output_tex = prm::get_texture({
		.name = CSTR("Color Output"),
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
				 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		.dimensions = {Window::width(), Window::height(), 1},
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.initial_layout = VK_IMAGE_LAYOUT_GENERAL,
	});
	integrator->scene_ubo_buffers.resize(vk::MAX_FRAMES_IN_FLIGHT);
	for (u64 i = 0; i < integrator->scene_ubo_buffers.size; i++) {
		lm::String buffer_name = lm::str_concat(integrator->arena, CSTR("Scene UBO #"),
												lm::str_from_u64(integrator->arena, i), /*cstr=*/true);
		integrator->scene_ubo_buffers[i] = prm::get_buffer({
			.name = buffer_name,
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.memory_type = vk::BUFFER_TYPE_CPU_TO_GPU,
			.size = sizeof(SceneUBO),
		});
	}
	update_uniform_buffers(integrator);
	upload_scene_ubo(integrator);
}

static bool common_gui(Integrator*) {
	ImGui::NewLine();
	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	ImGui::Text("Integrator settings:");
	ImGui::PopStyleColor();
	return false;
}

static bool common_update(Integrator* integrator) {
	f32 trans_speed = 0.01f;
	lm::vec3 front;
	if (Window::is_key_held(KeyInput::KEY_LEFT_SHIFT)) {
		trans_speed *= 4;
	}
	lm::Camera& camera = integrator->lumen_scene->camera;
	front.x = cos(lm::radians(camera.rotation.x)) * sin(lm::radians(camera.rotation.y));
	front.y = sin(lm::radians(camera.rotation.x));
	front.z = cos(lm::radians(camera.rotation.x)) * cos(lm::radians(camera.rotation.y));
	front = lm::normalize(-front);
	if (Window::is_key_held(KeyInput::KEY_W)) {
		camera.position += front * trans_speed;
		integrator->updated = true;
	}
	if (Window::is_key_held(KeyInput::KEY_A)) {
		camera.position -= lm::normalize(lm::cross(front, lm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
		integrator->updated = true;
	}
	if (Window::is_key_held(KeyInput::KEY_S)) {
		camera.position -= front * trans_speed;
		integrator->updated = true;
	}
	if (Window::is_key_held(KeyInput::KEY_D)) {
		camera.position += lm::normalize(lm::cross(front, lm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
		integrator->updated = true;
	}
	if (Window::is_key_held(KeyInput::SPACE) || Window::is_key_held(KeyInput::KEY_E)) {
		lm::vec3 right = lm::normalize(lm::cross(front, lm::vec3(0.0f, 1.0f, 0.0f)));
		lm::vec3 up = lm::cross(right, front);
		camera.position += up * trans_speed;
		integrator->updated = true;
	}
	if (Window::is_key_held(KeyInput::KEY_LEFT_CONTROL) || Window::is_key_held(KeyInput::KEY_Q)) {
		lm::vec3 right = lm::normalize(lm::cross(front, lm::vec3(0.0f, 1.0f, 0.0f)));
		lm::vec3 up = lm::cross(right, front);
		camera.position -= up * trans_speed;
		integrator->updated = true;
	}

	if (Window::is_mouse_held(MouseAction::LEFT, integrator->scene_ubo.clicked_pos) &&
		Window::is_key_held(KeyInput::KEY_TAB)) {
		integrator->scene_ubo.debug_click = 1;
	} else {
		integrator->scene_ubo.debug_click = 0;
	}
	update_uniform_buffers(integrator);
	return integrator->updated;
}

static void common_destroy(Integrator* integrator, bool resize) {
	for (vk::Buffer*& scene_ubo_buffer : integrator->scene_ubo_buffers) {
		prm::remove(scene_ubo_buffer);
		scene_ubo_buffer = nullptr;
	}
	integrator->scene_ubo_buffers.clear();
	prm::remove(integrator->lumen_scene->scene_desc_buffer);
	prm::remove(integrator->output_tex);
	integrator->scene_ubo_buffer = nullptr;
	integrator->lumen_scene->scene_desc_buffer = nullptr;
	integrator->output_tex = nullptr;
	integrator->initialized = false;
}

static void default_create_accel(Integrator* integrator, vk::BVH* tlas, lm::Array<vk::BVH>* blases) {
	if (!blases->initialized()) {
		*blases = lm::array_create<vk::BVH>(integrator->arena, integrator->lumen_scene->prim_meshes.size);
	}
	blases->resize_with_value(integrator->lumen_scene->prim_meshes.size);

	lm::ScratchArena scratch = integrator->arena;
	auto blas_inputs = lm::fixed_array_create<vk::BlasInput>(scratch.arena, integrator->lumen_scene->prim_meshes.size);
	VkDeviceAddress vertex_address = integrator->lumen_scene->vertex_buffer->device_address();
	VkDeviceAddress idx_address = integrator->lumen_scene->index_buffer->device_address();
	for (auto& prim_mesh : integrator->lumen_scene->prim_meshes) {
		vk::BlasInput geo = vk::blas_input_create(prim_mesh.vtx_count, prim_mesh.idx_count, prim_mesh.vtx_offset,
												  prim_mesh.first_idx, vertex_address, sizeof(Vertex), idx_address);
		blas_inputs.push_back({geo});
	}
	vk::blas_build(scratch, blases->to_slice(), blas_inputs.to_slice(),
				   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
					   VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
	auto tlas_instances = lm::fixed_array_create<VkAccelerationStructureInstanceKHR>(
		scratch.arena, integrator->lumen_scene->prim_meshes.size);
	for (const auto& pm : integrator->lumen_scene->prim_meshes) {
		VkAccelerationStructureInstanceKHR ray_inst{};
		ray_inst.transform = vk::to_vk_matrix(pm.world_matrix);
		ray_inst.instanceCustomIndex = pm.prim_idx;
		assert(pm.prim_idx < blases->size);
		ray_inst.accelerationStructureReference = (*blases)[pm.prim_idx].device_address();
		ray_inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		ray_inst.mask = 0xFF;
		ray_inst.instanceShaderBindingTableRecordOffset = 0;
		tlas_instances.push_back(ray_inst);
	}
	vk::tlas_build(*tlas, tlas_instances.to_slice(), VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void set_type(Integrator* integrator, IntegratorType type) {
	integrator->type = type;
	integrator->gui = no_gui;
	integrator->create_accel = default_create_accel;
	switch (type) {
		case INTEGRATOR_PATH:
			integrator->init = path::init;
			integrator->render = path::render;
			integrator->update = path::update;
			integrator->gui = path::gui;
			integrator->destroy = path::destroy;
			break;
		case INTEGRATOR_BDPT:
			integrator->init = bdpt::init;
			integrator->render = bdpt::render;
			integrator->update = bdpt::update;
			integrator->destroy = bdpt::destroy;
			break;
		case INTEGRATOR_SPPM:
			integrator->init = sppm::init;
			integrator->render = sppm::render;
			integrator->update = sppm::update;
			integrator->destroy = sppm::destroy;
			break;
		case INTEGRATOR_VCM:
			integrator->init = vcm::init;
			integrator->render = vcm::render;
			integrator->update = vcm::update;
			integrator->gui = vcm::gui;
			integrator->destroy = vcm::destroy;
			break;
		case INTEGRATOR_PSSMLT:
			integrator->init = pssmlt::init;
			integrator->render = pssmlt::render;
			integrator->update = pssmlt::update;
			integrator->destroy = pssmlt::destroy;
			break;
		case INTEGRATOR_SMLT:
			integrator->init = smlt::init;
			integrator->render = smlt::render;
			integrator->update = smlt::update;
			integrator->destroy = smlt::destroy;
			break;
		case INTEGRATOR_VCMMLT:
			integrator->init = vcmmlt::init;
			integrator->render = vcmmlt::render;
			integrator->update = vcmmlt::update;
			integrator->gui = vcmmlt::gui;
			integrator->destroy = vcmmlt::destroy;
			break;
		case INTEGRATOR_RESTIR:
			integrator->init = restir::init;
			integrator->render = restir::render;
			integrator->update = restir::update;
			integrator->gui = restir::gui;
			integrator->destroy = restir::destroy;
			break;
		case INTEGRATOR_RESTIRGI:
			integrator->init = restirgi::init;
			integrator->render = restirgi::render;
			integrator->update = restirgi::update;
			integrator->gui = restirgi::gui;
			integrator->destroy = restirgi::destroy;
			break;
		case INTEGRATOR_DDGI:
			integrator->init = ddgi::init;
			integrator->render = ddgi::render;
			integrator->update = ddgi::update;
			integrator->gui = ddgi::gui;
			integrator->destroy = ddgi::destroy;
			integrator->create_accel = ddgi::create_accel;
			break;
		case INTEGRATOR_RESTIRPT:
			integrator->init = restirpt::init;
			integrator->render = restirpt::render;
			integrator->update = restirpt::update;
			integrator->gui = restirpt::gui;
			integrator->destroy = restirpt::destroy;
			break;
		case INTEGRATOR_IRCACHE:
			integrator->init = ircache::init;
			integrator->render = ircache::render;
			integrator->update = ircache::update;
			integrator->gui = ircache::gui;
			integrator->destroy = ircache::destroy;
			break;
		default:
			assert(false);
	}
	assert(integrator->init && integrator->render && integrator->update && integrator->gui && integrator->destroy &&
		   integrator->create_accel);
}

void init(Integrator* integrator) {
	common_init(integrator);
	integrator->init(integrator);
	integrator->initialized = true;
}

void render(Integrator* integrator) {
	upload_scene_ubo(integrator);
	integrator->render(integrator);
}

bool update(Integrator* integrator) {
	common_update(integrator);
	return integrator->update(integrator);
}

bool gui(Integrator* integrator) {
	bool result = common_gui(integrator);
	result |= integrator->gui(integrator);
	return result;
}

void destroy(Integrator* integrator, bool resize) {
	if (!integrator->initialized) {
		return;
	}
	integrator->destroy(integrator, resize);
	common_destroy(integrator, resize);
}

void create_accel(Integrator* integrator, vk::BVH* tlas, lm::Array<vk::BVH>* blases) {
	integrator->create_accel(integrator, tlas, blases);
}

}  // namespace integrator
