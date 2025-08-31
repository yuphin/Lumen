#include "BBox.h"

namespace lm {
void bbox_insert(BBox& bbox, const glm::vec3& v) {
	bbox.min_corner = glm::min(bbox.min_corner, v);
	bbox.max_corner = glm::max(bbox.max_corner, v);
}
void bbox_insert(BBox& bbox, const BBox& b) {
	bbox_insert(bbox, b.min());
	bbox_insert(bbox, b.max());
}
BBox bbox_transform(BBox& bbox, const glm::mat4& mat) {
	glm::vec3 corners[8];
	corners[0] = mat * glm::vec4(bbox.min(), 1.0);
	corners[1] = mat * glm::vec4(bbox.min().x, bbox.min().y, bbox.max().z, 1.0);
	corners[2] = mat * glm::vec4(bbox.min().x, bbox.max().y, bbox.min().z, 1.0);
	corners[3] = mat * glm::vec4(bbox.min().x, bbox.max().y, bbox.max().z, 1.0);
	corners[4] = mat * glm::vec4(bbox.max().x, bbox.min().y, bbox.min().z, 1.0);
	corners[5] = mat * glm::vec4(bbox.max().x, bbox.min().y, bbox.max().z, 1.0);
	corners[6] = mat * glm::vec4(bbox.max().x, bbox.max().y, bbox.min().z, 1.0);
	corners[7] = mat * glm::vec4(bbox.max(), 1.0);
	BBox result;
	for (const auto& c : corners) {
		bbox_insert(result, c);
	}
	return result;
}
}  // namespace lm
