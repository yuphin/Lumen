
#include "Camera2.h"
namespace lm {

static constexpr glm::vec3 UP = glm::vec3(0, 1, 0);
static constexpr glm::vec3 RIGHT = glm::vec3(1, 0, 0);
static constexpr glm::vec3 FORWARD = glm::vec3(0, 0, 1);
static void camera_init_projection(Camera* camera, f32 aspect_ratio) {
	camera->projection[0][0] = 1 / (aspect_ratio * tanf(glm::radians(camera->fov / 2)));
	camera->projection[1][1] = -1 / (tanf(glm::radians(camera->fov / 2)));
	camera->projection[2][2] = camera->far_plane / (camera->near_plane - camera->far_plane);
	camera->projection[2][3] = -1;
	camera->projection[3][2] = camera->near_plane * camera->far_plane / (camera->near_plane - camera->far_plane);
}

void camera_init(Camera* camera, f32 fov, f32 cam_near, f32 cam_far, f32 aspect_ratio, const glm::vec3& dir,
				 const glm::vec3& pos, const glm::vec3 rot) {
	camera->fov = fov;
	camera->near_plane = cam_near;
	camera->far_plane = cam_far;
	camera->position = pos;
	camera->direction = dir;
	// camera->view = glm::lookAtLH(pos, pos + dir, glm::vec3(0, 1, 0));
	camera->rotation = rot;
	camera_init_projection(camera, aspect_ratio);
	camera_update_view(camera);
}

void camera_update_view(Camera* camera) {
	glm::mat4 camera_matrix = glm::mat4(1);
	camera_matrix = glm::translate(camera_matrix, camera->position);
	camera_matrix = glm::rotate(camera_matrix, glm::radians(camera->rotation.y), UP);
	camera_matrix = glm::rotate(camera_matrix, glm::radians(camera->rotation.x), RIGHT);
	camera_matrix = glm::rotate(camera_matrix, glm::radians(camera->rotation.z), FORWARD);
	camera->view = glm::inverse(camera_matrix);
	camera->direction = glm::vec3(-camera->view[0][2], -camera->view[1][2], camera->view[2][2]);
}

void camera_rotate(Camera* camera, f32 rx, f32 ry, f32 rz) {
		camera->rotation.x += rx;
		camera->rotation.y += ry;
		camera->rotation.z += rz;
		camera->rotation = glm::fmod(camera->rotation, glm::vec3(360.0f));
}

}  // namespace lm
