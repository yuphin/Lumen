#pragma once
#include "LumenPCH.h"
#include <tiny_obj_loader.h>

struct CameraConfiguration {
	float fov;
	glm::vec3 pos;
};

struct LumenPrimMesh {
	std::string name;
	int material_idx;
	int vtx_offset;
	int first_idx;
	glm::mat4 world_matrix;
};

struct LumenMaterial {
	glm::vec3 albedo;
	glm::vec3 emissive_factor;
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
	std::vector<LumenMaterial> materials;
};

