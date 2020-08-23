#include "lmhpch.h"
#include "Camera.h"

void Camera::update_view_matrix() {

	constexpr glm::vec3 RIGHT = glm::vec3(1, 0, 0);
	constexpr glm::vec3 UP = glm::vec3(0, 1, 0);
	constexpr glm::vec3 FORWARD = glm::vec3(0, 0, 1);
	auto res = glm::mat4{ 1 };
	res = glm::translate(res, position);
	res = glm::rotate(res, glm::radians(rotation.x), RIGHT);
	res = glm::rotate(res, glm::radians(rotation.y), UP);
	res = glm::rotate(res, glm::radians(rotation.z), FORWARD);
	view = glm::inverse(res);
}

