#pragma once
#include "LumenPCH.h"

class Camera {
public:
	enum class CameraType {
		FPS,
		LookAt
	};

	explicit Camera(float cam_near, float cam_far) :
		cam_near(cam_near), cam_far(cam_far) {
		this->update_view_matrix();
	}
	inline void set_position(const glm::vec3& pos) {
		this->position = pos;
	}
	inline void set_rotation(const glm::vec3& rot) {
		this->rotation = rot;
	}
	inline void translate(float dx, float dy, float dz) {
		this->position.x += dx;
		this->position.y += dy;
		this->position.z += dz;
	}

	inline void translate(const glm::vec3& delta) {
		this->position += delta;
	}

	inline void rotate(float rx, float ry, float rz) {
		this->rotation.x += rx;
		this->rotation.y += ry;
		this->rotation.z += rz;
	}

	inline void rotate(const glm::vec3& delta) {
		this->rotation += delta;
	}
	void update_view_matrix() {
		constexpr glm::vec3 UP = glm::vec3(0, 1, 0);
		constexpr glm::vec3 RIGHT = glm::vec3(1, 0, 0);
		constexpr glm::vec3 FORWARD = glm::vec3(0, 0, 1);
		auto res = glm::mat4{ 1 };
		res = glm::translate(res, position);
		res = glm::rotate(res, glm::radians(rotation.y), UP);
		res = glm::rotate(res, glm::radians(rotation.x), RIGHT);
		res = glm::rotate(res, glm::radians(rotation.z), FORWARD);
		view = glm::inverse(res);
	}
	glm::mat4 projection{ 1.f };
	glm::mat4 view{ 1.f };
	float cam_near, cam_far;
	CameraType type = CameraType::FPS;
	glm::vec3 position{}, rotation{};
protected:
	virtual void make_projection_matrix(bool use_fov) = 0;
private:
};

class PerspectiveCamera : public Camera {
public:
	explicit PerspectiveCamera(float fov, float cam_near, float cam_far, float aspect_ratio,
							   const glm::vec3 pos = glm::vec3(0.0f)) :
		fov(fov), aspect_ratio(aspect_ratio), Camera(cam_near, cam_far) {
		left = right = top = bot = -1;
		this->make_projection_matrix(true);
		this->set_position(pos);
	}
	explicit PerspectiveCamera(float left, float right, float top, float bot, float cam_near, float cam_far,
							   const glm::vec3 pos = glm::vec3(0.0f)) :
		left(left), right(right), top(top), bot(bot), Camera(cam_near, cam_far) {
		fov = aspect_ratio = -1;
		this->make_projection_matrix();
		this->set_position(pos);
	}
private:
	void make_projection_matrix(bool use_fov = false) override {

		if(use_fov) {
			projection[0][0] = 1 / (aspect_ratio * tanf(glm::radians(fov) / 2));
			projection[1][1] = -1 / (tanf(glm::radians(fov) / 2));
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

