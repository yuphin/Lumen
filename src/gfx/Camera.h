#pragma once
#include "lmhpch.h"
class Camera {
public:
	explicit Camera(float cam_near, float cam_far, float scale = 1) :
			cam_near(cam_near), cam_far(cam_far), scale(scale) {
		update_view_matrix();
	}
	inline void set_pos(const glm::vec3& pos) {
		this->position = pos;
	}
	inline void set_rot(const glm::vec3& rot) {
		this->rotation = rot;
	}
	inline void set_scale(float scale) {
		this->scale = scale;
	}

	void update_view_matrix();
	glm::mat4 proj{1.f};
	glm::mat4 view{1.f};
	glm::mat4 vp{1.f};

protected:
	float cam_near, cam_far, scale;
	glm::vec3 position{}, rotation{};
	virtual void make_projection_matrix(bool use_fov) = 0;
private:
};

