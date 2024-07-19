#pragma once
#include "../LumenPCH.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace lumen {
class Camera {
   public:
	enum class CameraType { FPS, LookAt };

	explicit Camera(float cam_near, float cam_far) : cam_near(cam_near), cam_far(cam_far) {}
	inline void set_position(const glm::vec3& pos) { this->position = pos; }
	inline void set_direction(const glm::vec3& dir) { this->direction = dir; }
	inline void set_rotation(const glm::vec3& rot) { this->rotation = rot; }
	inline void translate(float dx, float dy, float dz) {
		position.x += dx;
		position.y += dy;
		position.z += dz;
	}

	inline void translate(const glm::vec3& delta) { this->position += delta; }

	inline void rotate(float rx, float ry, float rz) {
		rotation.x += rx;
		rotation.y += ry;
		rotation.z += rz;
	}

	inline void rotate(const glm::vec3& delta) { this->rotation += delta; }
	void update_view_matrix() {
		constexpr glm::vec3 UP = glm::vec3(0, 1, 0);
		constexpr glm::vec3 RIGHT = glm::vec3(1, 0, 0);
		constexpr glm::vec3 FORWARD = glm::vec3(0, 0, 1);
		auto res = glm::mat4{1};
		res = glm::translate(res, position);
		res = glm::rotate(res, glm::radians(rotation.y), UP);
		res = glm::rotate(res, glm::radians(rotation.x), RIGHT);
		res = glm::rotate(res, glm::radians(rotation.z), FORWARD);
		view = glm::inverse(res);
		camera = res;
	}
	glm::mat4 projection{1.f};
	glm::mat4 view{1.f}, camera{1.f};
	float cam_near, cam_far;
	CameraType type = CameraType::FPS;
	glm::vec3 position{}, rotation{}, direction{};

   private:
};

class PerspectiveCamera : public Camera {
   public:
	explicit PerspectiveCamera(float fov, float cam_near, float cam_far, float aspect_ratio, const glm::vec3& pos)
		: fov(fov), aspect_ratio(aspect_ratio), Camera(cam_near, cam_far) {
		left = right = top = bot = -1;
		make_projection_matrix(true);
		set_position(pos);
		update_view_matrix();
	}
	explicit PerspectiveCamera(float left, float right, float top, float bot, float cam_near, float cam_far,
							   const glm::vec3& pos = glm::vec3(0.0f))
		: left(left), right(right), top(top), bot(bot), Camera(cam_near, cam_far) {
		fov = aspect_ratio = -1;
		make_projection_matrix();
		set_position(pos);
		update_view_matrix();
	}

	explicit PerspectiveCamera(float fov, float cam_near, float cam_far, float aspect_ratio, const glm::vec3& dir,
							   const glm::vec3& pos)
		: fov(fov), aspect_ratio(aspect_ratio), Camera(cam_near, cam_far) {
		left = right = top = bot = -1;
		make_projection_matrix(true);
		set_position(pos);
		set_direction(dir);
		view = glm::lookAtLH(position, position + direction, glm::vec3(0, 1, 0));
		glm::vec3 scale;
		glm::quat q;
		glm::vec3 translation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(view, scale, q, translation, skew, perspective);
		glm::vec3 rot{};
		glm::extractEulerAngleXYZ(glm::toMat4(q), rot.x, rot.y, rot.z);
		rot *= 180. / glm::pi<float>();
		rotation = rot;
	}

	explicit PerspectiveCamera(float fov, const glm::mat4 cam_matrix, float cam_near, float cam_far, float aspect_ratio)
		: fov(fov), aspect_ratio(aspect_ratio), Camera(cam_near, cam_far) {
		left = right = top = bot = -1;
		this->make_projection_matrix(true);
		camera = cam_matrix;
		view = glm::inverse(cam_matrix);
		glm::vec3 scale;
		glm::quat q;
		glm::vec3 translation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(view, scale, q, translation, skew, perspective);
		glm::vec3 rot{};
		glm::extractEulerAngleXYZ(glm::toMat4(q), rot.x, rot.y, rot.z);
		rot *= 180. / glm::pi<float>();
		rotation = rot;
		glm::vec3 pos = glm::vec3({cam_matrix[0][3], cam_matrix[1][3], cam_matrix[2][3]});
		this->set_position(pos);
	}

   private:
	void make_projection_matrix(bool use_fov = false) {
		if (use_fov) {
			projection[0][0] = 1 / (aspect_ratio * tanf(glm::radians(fov / 2)));
			projection[1][1] = -1 / (tanf(glm::radians(fov / 2)));
			projection[2][2] = cam_far / (cam_near - cam_far);
			projection[2][3] = -1;
			projection[3][2] = cam_near * cam_far / (cam_near - cam_far);
		} else {
			projection[0][0] = 2 / (right - left);
			projection[1][1] = -2 / (top - bot);
			projection[2][2] = cam_far / (cam_near - cam_far);
			projection[2][3] = -1;
			projection[3][2] = cam_near * cam_far / (cam_near - cam_far);
		}
	}

	float fov{}, aspect_ratio{};
	float left{}, right{}, top{}, bot{};
};

}  // namespace lumen
