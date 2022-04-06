#pragma once
#include "LumenPCH.h"
#include <tiny_obj_loader.h>
#include "shaders/commons.h"
#include "Framework/MitsubaParser.h"

struct CameraConfiguration {
	float fov;
	glm::vec3 pos;
	glm::vec3 dir;
};

struct SceneConfig {
	std::string filename;
};

struct LumenPrimMesh {
	std::string name;
	uint32_t material_idx;
	uint32_t vtx_offset;
	uint32_t first_idx;
	uint32_t idx_count;
	uint32_t vtx_count;
	uint32_t prim_idx;
	glm::mat4 world_matrix;
	glm::vec3 min_pos;
	glm::vec3 max_pos;
};

//struct LumenMaterial {
//	glm::vec3 albedo;
//	glm::vec3 emissive_factor;
//	float ior;
//	glm::vec3 metalness;
//	float roughness;
//	uint32_t bsdf_type;
//	// Disney BSDF
//	float metallic;
//	float specular_tint;
//	float sheen_tint;
//	float specular;
//	float clearcoat;
//	float clearcoat_gloss;
//	float sheen;
//	float subsurface;
//};

struct LumenLight {
	glm::vec3 pos;
	glm::vec3 to;
	glm::vec3 L;
	uint32_t light_type;
    float world_radius;
};
class LumenScene {
public:
	void load_scene(const std::string& root, const std::string& filename);
	CameraConfiguration cam_config;
	std::vector<glm::vec3> positions;
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec4> tangents;
	std::vector<glm::vec2> texcoords0;
	std::vector<glm::vec2> texcoords1;
	std::vector<glm::vec4> colors0;
	std::vector<LumenPrimMesh> prim_meshes;
	std::vector<Material> materials;
	std::vector<LumenLight> lights;
	struct Dimensions {
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 max = glm::vec3(std::numeric_limits<float>::min());
		glm::vec3 size{ 0.f };
		glm::vec3 center{ 0.f };
		float radius{ 0 };
	} m_dimensions;
private:
	void load_obj(const std::string& path);
	void compute_scene_dimensions();
};

