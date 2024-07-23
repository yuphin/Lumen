#include <LumenPCH.h>
struct Bbox {
	Bbox() = default;
	Bbox(glm::vec3 _min, glm::vec3 _max) : m_min(_min), m_max(_max) {}
	Bbox(const std::vector<glm::vec3>& corners) {
		for (auto& c : corners) {
			insert(c);
		}
	}

	void insert(const glm::vec3& v) {
		m_min = {std::min(m_min.x, v.x), std::min(m_min.y, v.y), std::min(m_min.z, v.z)};
		m_max = {std::max(m_max.x, v.x), std::max(m_max.y, v.y), std::max(m_max.z, v.z)};
	}

	void insert(const Bbox& b) {
		insert(b.m_min);
		insert(b.m_max);
	}

	inline Bbox& operator+=(float v) {
		m_min -= v;
		m_max += v;
		return *this;
	}

	inline bool is_empty() const {
		return m_min == glm::vec3{std::numeric_limits<float>::max()} ||
			   m_max == glm::vec3{std::numeric_limits<float>::lowest()};
	}
	inline uint32_t rank() const {
		uint32_t result{0};
		result += m_min.x < m_max.x;
		result += m_min.y < m_max.y;
		result += m_min.z < m_max.z;
		return result;
	}
	inline bool isPoint() const { return m_min == m_max; }
	inline bool isLine() const { return rank() == 1u; }
	inline bool isPlane() const { return rank() == 2u; }
	inline bool isVolume() const { return rank() == 3u; }
	inline glm::vec3 min() { return m_min; }
	inline glm::vec3 max() { return m_max; }
	inline glm::vec3 extents() { return m_max - m_min; }
	inline glm::vec3 center() { return (m_min + m_max) * 0.5f; }
	inline float radius() { return glm::length(m_max - m_min) * 0.5f; }

	Bbox transform(glm::mat4 mat) {
		std::vector<glm::vec3> corners(8);
		corners[0] = mat * glm::vec4(m_min.x, m_min.y, m_min.z, 1);
		corners[1] = mat * glm::vec4(m_min.x, m_min.y, m_max.z, 1);
		corners[2] = mat * glm::vec4(m_min.x, m_max.y, m_min.z, 1);
		corners[3] = mat * glm::vec4(m_min.x, m_max.y, m_max.z, 1);
		corners[4] = mat * glm::vec4(m_max.x, m_min.y, m_min.z, 1);
		corners[5] = mat * glm::vec4(m_max.x, m_min.y, m_max.z, 1);
		corners[6] = mat * glm::vec4(m_max.x, m_max.y, m_min.z, 1);
		corners[7] = mat * glm::vec4(m_max.x, m_max.y, m_max.z, 1);

		Bbox result(corners);
		return result;
	}

   private:
	glm::vec3 m_min{std::numeric_limits<float>::max()};
	glm::vec3 m_max{std::numeric_limits<float>::lowest()};
};