namespace lm {
struct BBox {
	lm::vec3 min_corner{F32_MAX};
	lm::vec3 max_corner{F32_MIN};
	BBox() = default;
	BBox(lm::vec3 min, lm::vec3 max) : min_corner(min), max_corner(max) {}
	inline bool is_empty() const { return min_corner == lm::vec3{F32_MAX} || max_corner == lm::vec3{F32_MIN}; }
	inline lm::vec3 min() const { return min_corner; }
	inline lm::vec3 max() const { return max_corner; }
	inline lm::vec3 extents() const { return max_corner - min_corner; }
	inline lm::vec3 center() const { return (min_corner + max_corner) * 0.5f; }
	inline f32 radius() const { return lm::length(max_corner - min_corner) * 0.5f; }
};

void bbox_insert(BBox& bbox, const lm::vec3& v);
void bbox_insert(BBox& bbox, const BBox& b);
BBox bbox_transform(BBox& bbox, const lm::mat4& mat);
}  // namespace lm
