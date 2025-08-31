namespace lm {
struct BBox {
	glm::vec3 min_corner{F32_MAX};
	glm::vec3 max_corner{F32_MIN};
	BBox() = default;
	BBox(glm::vec3 min, glm::vec3 max) : min_corner(min), max_corner(max) {}
	inline bool is_empty() const { return min_corner == glm::vec3{F32_MAX} || max_corner == glm::vec3{F32_MIN}; }
	inline glm::vec3 min() const { return min_corner; }
	inline glm::vec3 max() const { return max_corner; }
	inline glm::vec3 extents() const { return max_corner - min_corner; }
	inline glm::vec3 center() const { return (min_corner + max_corner) * 0.5f; }
	inline f32 radius() const { return glm::length(max_corner - min_corner) * 0.5f; }
};

void bbox_insert(BBox& bbox, const glm::vec3& v);
void bbox_insert(BBox& bbox, const BBox& b);
BBox bbox_transform(BBox& bbox, const glm::mat4& mat);
}  // namespace lm
