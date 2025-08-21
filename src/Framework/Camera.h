#pragma once

namespace lm {
class Camera {
   public:
	enum class CameraType { FPS, LookAt };

	explicit Camera(f32 cam_near, f32 cam_far, f32 fov) : fov(fov), cam_near(cam_near), cam_far(cam_far) {}
	inline void translate(f32 dx, f32 dy, f32 dz) {
		position.x += dx;
		position.y += dy;
		position.z += dz;
	}

	inline void translate(const glm::vec3& delta) { this->position += delta; }

	inline void rotate(f32 rx, f32 ry, f32 rz) {
		rotation.x += rx;
		rotation.y += ry;
		rotation.z += rz;
		rotation = glm::fmod(rotation, glm::vec3(360.0f));
	}

	inline void rotate(const glm::vec3& delta) {
		this->rotation += delta;
		rotation = glm::fmod(rotation, glm::vec3(360.0f));
	}
	void update_view_matrix() {
		constexpr glm::vec3 UP = glm::vec3(0, 1, 0);
		constexpr glm::vec3 RIGHT = glm::vec3(1, 0, 0);
		constexpr glm::vec3 FORWARD = glm::vec3(0, 0, 1);
		auto res = glm::mat4(1);
		res = glm::translate(res, position);
		res = glm::rotate(res, glm::radians(rotation.y), UP);
		res = glm::rotate(res, glm::radians(rotation.x), RIGHT);
		res = glm::rotate(res, glm::radians(rotation.z), FORWARD);
		view = glm::inverse(res);
		direction = glm::vec3(-view[0][2], -view[1][2], view[2][2]);
		camera = res;
	}
	glm::mat4 projection{1.f};
	glm::mat4 view{1.f}, camera{1.f};
	glm::vec3 position{}, rotation{}, direction{};
	f32 fov;
	f32 cam_near;
	f32 cam_far;

};

class PerspectiveCamera : public Camera {
   public:
	explicit PerspectiveCamera(f32 fov, f32 cam_near, f32 cam_far, f32 aspect_ratio, const glm::vec3& dir,
							   const glm::vec3& pos, const glm::vec3 rot)
		: Camera(cam_near, cam_far, fov) {
		position = pos;
		direction = dir;
		rotation = rot;
		make_projection_matrix(aspect_ratio);
		update_view_matrix();
	}


   private:
	void make_projection_matrix(f32 aspect_ratio) {
		projection[0][0] = 1 / (aspect_ratio * tanf(glm::radians(fov / 2)));
		projection[1][1] = -1 / (tanf(glm::radians(fov / 2)));
		projection[2][2] = cam_far / (cam_near - cam_far);
		projection[2][3] = -1;
		projection[3][2] = cam_near * cam_far / (cam_near - cam_far);
	}
};

}  // namespace lm
