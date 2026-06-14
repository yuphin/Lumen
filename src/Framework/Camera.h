#pragma once

namespace lm {

struct Camera {
	lm::mat4 projection;
	lm::mat4 view;
	lm::vec3 position;
	// In degrees
	lm::vec3 rotation;
	lm::vec3 direction;
	f32 fov;
	f32 near_plane;
	f32 far_plane;
};

void camera_init(Camera* camera, f32 fov, f32 cam_near, f32 cam_far, f32 aspect_ratio, const lm::vec3& dir,
				 const lm::vec3& pos, const lm::vec3 rot);
void camera_rotate(Camera* camera);
void camera_update_view(Camera* camera);
void camera_rotate(Camera* camera, f32 rx, f32 ry, f32 rz);

}  // namespace lm
