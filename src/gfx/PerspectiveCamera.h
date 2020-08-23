#pragma once
#include "Camera.h"
#include "lmhpch.h"
class PerspectiveCamera : public Camera
{
public:
	explicit PerspectiveCamera(float fov, float cam_near, float cam_far, float aspect_ratio, float scale = 1) :
		fov(fov), aspect_ratio(aspect_ratio), Camera(cam_near, cam_far, scale) {
		left = right = top = bot = -1;
		make_projection_matrix(true);
		this->vp = proj * view;
	}
	explicit PerspectiveCamera(float left, float right, float top, float bot,
		float cam_near, float cam_far, float scale = 1) :
		left(left), right(right), top(top), bot(bot), Camera(cam_near, cam_far, scale) {
		fov = aspect_ratio = -1;
		make_projection_matrix();
		this->vp = proj * view;
	}
private:
	float fov{}, aspect_ratio{};
	float left{}, right{}, top{}, bot{};
	virtual void make_projection_matrix(bool use_fov = false) override;

};

