#pragma once

namespace lm {
class Camera {
   public:
	enum class CameraType { FPS, LookAt };

	explicit Camera(f32 cam_near, f32 cam_far) : cam_near(cam_near), cam_far(cam_far) {}
	inline void set_position(const glm::vec3& pos) { this->position = pos; }
	inline void set_direction(const glm::vec3& dir) { this->direction = dir; }
	inline void set_rotation(const glm::vec3& rot) { this->rotation = rot; }
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
		auto res = glm::mat4{1};
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
	f32 cam_near, cam_far;
	CameraType type = CameraType::FPS;
	glm::vec3 position{}, rotation{}, direction{};

   private:
};

class PerspectiveCamera : public Camera {
   public:
	explicit PerspectiveCamera(f32 fov, f32 cam_near, f32 cam_far, f32 aspect_ratio, const glm::vec3& pos)
		: Camera(cam_near, cam_far), fov(fov), aspect_ratio(aspect_ratio) {
		left = right = top = bot = -1;
		make_projection_matrix(true);
		set_position(pos);
		update_view_matrix();
	}
	explicit PerspectiveCamera(f32 left, f32 right, f32 top, f32 bot, f32 cam_near, f32 cam_far,
							   const glm::vec3& pos = glm::vec3(0.0f))
		: Camera(cam_near, cam_far), left(left), right(right), top(top), bot(bot) {
		fov = aspect_ratio = -1;
		make_projection_matrix();
		set_position(pos);
		update_view_matrix();
	}

	explicit PerspectiveCamera(f32 fov, f32 cam_near, f32 cam_far, f32 aspect_ratio, const glm::vec3& dir,
							   const glm::vec3& pos)
		: Camera(cam_near, cam_far), fov(fov), aspect_ratio(aspect_ratio) {
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
		rot *= 180. / glm::pi<f32>();
		rotation = rot;
	}

	explicit PerspectiveCamera(f32 fov, const glm::mat4 cam_matrix, f32 cam_near, f32 cam_far, f32 aspect_ratio)
		: Camera(cam_near, cam_far), fov(fov), aspect_ratio(aspect_ratio) {
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
		rot *= 180. / glm::pi<f32>();
		rotation = rot;
		glm::vec3 pos = glm::vec3({cam_matrix[0][3], cam_matrix[1][3], cam_matrix[2][3]});
		this->set_position(pos);
	}

	f32 fov{}, aspect_ratio{};

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

	f32 left{}, right{}, top{}, bot{};
};

}  // namespace lm
