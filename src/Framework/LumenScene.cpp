#include "LumenPCH.h"
#include "LumenScene.h"
#include <json.hpp>
#include <tiny_obj_loader.h>
#include "shaders/commons.h"

struct Bbox {
	Bbox() = default;
	Bbox(glm::vec3 _min, glm::vec3 _max) : m_min(_min), m_max(_max) {}
	Bbox(const std::vector<glm::vec3>& corners) {
		for (auto& c : corners) {
			insert(c);
		}
	}

	void insert(const glm::vec3& v) {
		m_min = { std::min(m_min.x, v.x), std::min(m_min.y, v.y),
				 std::min(m_min.z, v.z) };
		m_max = { std::max(m_max.x, v.x), std::max(m_max.y, v.y),
				 std::max(m_max.z, v.z) };
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
		return m_min == glm::vec3{ std::numeric_limits<float>::max() } ||
			m_max == glm::vec3{ std::numeric_limits<float>::lowest() };
	}
	inline uint32_t rank() const {
		uint32_t result{ 0 };
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
	glm::vec3 m_min{ std::numeric_limits<float>::max() };
	glm::vec3 m_max{ std::numeric_limits<float>::lowest() };
};

using json = nlohmann::json;
void LumenScene::load_scene(const std::string& root, const std::string& filename) {
	std::ifstream i(root + filename);
	json j;
	i >> j;

	// Load obj file
	const std::string mesh_file = root + std::string(j["mesh_file"]);
	tinyobj::ObjReaderConfig reader_config;
	//reader_config.mtl_search_path = "./"; // Path to material files

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(mesh_file, reader_config)) {
		if (!reader.Error().empty()) {
			std::cerr << "TinyObjReader: " << reader.Error();
		}
		exit(1);
	}

	if (!reader.Warning().empty()) {
		std::cout << "TinyObjReader: " << reader.Warning();
	}

	auto& attrib = reader.GetAttrib();
	auto& shapes = reader.GetShapes();

	prim_meshes.resize(shapes.size());
	for (size_t s = 0; s < shapes.size(); s++) {
		prim_meshes[s].first_idx = indices.size();
		prim_meshes[s].vtx_offset = positions.size();
		prim_meshes[s].name = shapes[s].name;
		prim_meshes[s].idx_count = shapes[s].mesh.indices.size();
		prim_meshes[s].vtx_count = shapes[s].mesh.num_face_vertices.size();
		prim_meshes[s].prim_idx = s;
		glm::vec3 min_vtx = glm::vec3(FLT_MAX);
		glm::vec3 max_vtx = glm::vec3(-FLT_MAX);
		size_t index_offset = 0;
		uint32_t idx_val = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			for (size_t v = 0; v < 3; v++) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				indices.push_back(idx_val++);
				tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
				tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
				tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
				positions.push_back({ vx,vy,vz });
				min_vtx = glm::min(positions[positions.size() - 1], min_vtx);
				max_vtx = glm::max(positions[positions.size() - 1], max_vtx);
				if (idx.normal_index >= 0) {
					tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
					tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
					tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
					normals.push_back({ nx,ny,nz });
				}
				if (idx.texcoord_index >= 0) {
					tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
					tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
					texcoords0.push_back({ tx,ty });
				}
			}

			index_offset += 3;
			// per-face material
			shapes[s].mesh.material_ids[f];
		}
		prim_meshes[s].min_pos = min_vtx;
		prim_meshes[s].max_pos = max_vtx;
		prim_meshes[s].world_matrix = glm::mat4(1);
		// TODO: Implement world transforms


	}
	auto& bsdfs_arr = j["bsdfs"];
	materials.resize(bsdfs_arr.size());
	int bsdf_idx = 0;
	for (auto& bsdf : bsdfs_arr) {
		auto& refs = bsdf["refs"];
		if (!bsdf["albedo"].is_null()) {
			const auto& f = bsdf["albedo"];
			float f0 = f[0];
			float f1 = f[1];
			float f2 = f[2];
			materials[bsdf_idx].albedo = glm::vec3({ f[0], f[1], f[2] });
		}
		if (!bsdf["emissive_factor"].is_null()) {
			const auto& f = bsdf["emissive_factor"];
			materials[bsdf_idx].emissive_factor = glm::vec3({ f[0], f[1], f[2] });
		}

		if (bsdf["type"] == "lambertian") {
			materials[bsdf_idx].bsdf_type = BSDF_LAMBERTIAN;
		} else if (bsdf["type"] == "mirror") {
			materials[bsdf_idx].bsdf_type = BSDF_MIRROR;
		} else if (bsdf["type"] == "glass") {
			materials[bsdf_idx].bsdf_type = BSDF_GLASS;
			materials[bsdf_idx].ior = bsdf["ior"];
		}

		for (auto& ref : refs) {
			for (int s = 0; s < shapes.size(); s++) {
				if (ref == shapes[s].name) {
					prim_meshes[s].material_idx = bsdf_idx;
				}
			}
		}
		bsdf_idx++;
	}
	cam_config.fov = j["camera"]["fov"];
	const auto& p = j["camera"]["position"];
	cam_config.pos = { p[0], p[1], p[2] };
	compute_scene_dimensions();

}

void LumenScene::compute_scene_dimensions() {
	Bbox scene_bbox;
	for (const auto& pm : prim_meshes) {
		Bbox bbox(pm.min_pos, pm.max_pos);
		bbox.transform(pm.world_matrix);
		scene_bbox.insert(bbox);
	}
	if (scene_bbox.is_empty() || !scene_bbox.isVolume()) {
		LUMEN_WARN("glTF: Scene bounding box invalid, Setting to: [-1,-1,-1], "
				   "[1,1,1]");
		scene_bbox.insert({ -1.0f, -1.0f, -1.0f });
		scene_bbox.insert({ 1.0f, 1.0f, 1.0f });
	}
	m_dimensions.min = scene_bbox.min();
	m_dimensions.max = scene_bbox.max();
	m_dimensions.size = scene_bbox.extents();
	m_dimensions.center = scene_bbox.center();
	m_dimensions.radius = scene_bbox.radius();
}
