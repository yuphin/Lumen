#include "BBox.h"

namespace lm {
void bbox_insert(BBox& bbox, const lm::vec3& v) {
	bbox.min_corner = lm::min(bbox.min_corner, v);
	bbox.max_corner = lm::max(bbox.max_corner, v);
}
void bbox_insert(BBox& bbox, const BBox& b) {
	bbox_insert(bbox, b.min());
	bbox_insert(bbox, b.max());
}
BBox bbox_transform(BBox& bbox, const lm::mat4& mat) {
	lm::vec3 corners[8];
	corners[0] = mat * lm::vec4(bbox.min(), 1.0);
	corners[1] = mat * lm::vec4(bbox.min().x, bbox.min().y, bbox.max().z, 1.0);
	corners[2] = mat * lm::vec4(bbox.min().x, bbox.max().y, bbox.min().z, 1.0);
	corners[3] = mat * lm::vec4(bbox.min().x, bbox.max().y, bbox.max().z, 1.0);
	corners[4] = mat * lm::vec4(bbox.max().x, bbox.min().y, bbox.min().z, 1.0);
	corners[5] = mat * lm::vec4(bbox.max().x, bbox.min().y, bbox.max().z, 1.0);
	corners[6] = mat * lm::vec4(bbox.max().x, bbox.max().y, bbox.min().z, 1.0);
	corners[7] = mat * lm::vec4(bbox.max(), 1.0);
	BBox result;
	for (const auto& c : corners) {
		bbox_insert(result, c);
	}
	return result;
}
}  // namespace lm
