#include "lmhpch.h"
#include "PerspectiveCamera.h"

void PerspectiveCamera::make_projection_matrix(bool use_fov) {
	// Note that GLM is column-major ordered i.e
	//   0 4 8  12
	//   1 5 9  13
	//   2 6 10 14
	//   3 7 11 15
	if (use_fov) {
		proj[0][0] = 1 / (aspect_ratio * tanf(glm::radians(fov) / 2));
		proj[1][1] = -1 / (tanf(glm::radians(fov) / 2));
		proj[2][2] = cam_far / (cam_near - cam_far);
		proj[2][3] = -1;
		proj[3][2] = cam_near * cam_far / (cam_near - cam_far);
	}
	else {
		proj[0][0] = 2 / (right - left);
		proj[1][1] = -2 / (top - bot);
		proj[2][2] = cam_far / (cam_near - cam_far);
		proj[2][3] = -1;
		proj[3][2] = cam_near * cam_far / (cam_near - cam_far);
	}
}
