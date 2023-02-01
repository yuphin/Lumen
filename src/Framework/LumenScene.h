#pragma once
#include "LumenPCH.h"
#include <tiny_obj_loader.h>
#include "shaders/commons.h"
#include "Framework/MitsubaParser.h"

struct CameraSettings {
	float fov;
	glm::vec3 pos = vec3(0);
	glm::vec3 dir = vec3(0);
	glm::mat4 cam_matrix = glm::mat4();
};

enum class IntegratorType { Path, SER, BDPT, SPPM, VCM, PSSMLT, SMLT, VCMMLT, ReSTIR, ReSTIRGI, DDGI };

struct SceneConfig {
	IntegratorType integrator_type = IntegratorType::Path;
	int path_length = 6;
	vec3 sky_col = vec3(0, 0, 0);
	CameraSettings cam_settings;
	float base_radius = 0.03f;
	float radius_factor = 0.025f;
	float mutations_per_pixel = 100.0f;
	int num_bootstrap_samples = 360000;
	int num_mlt_threads = 360000;
	bool enable_vm = false;
	bool light_first = false;
	bool alternate = true;
	std::string integrator_name = "";
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

struct LumenLight {
	glm::vec3 pos;
	glm::vec3 to;
	glm::vec3 L;
	uint32_t light_flags;
	float world_radius;
};
class LumenScene {
   public:
	void load_scene(const std::string& path);
	std::vector<glm::vec3> positions;
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec4> tangents;
	std::vector<glm::vec2> texcoords0;
	std::vector<glm::vec2> texcoords1;
	std::vector<glm::vec4> colors0;
	std::vector<LumenPrimMesh> prim_meshes;
	std::vector<Material> materials;
	std::vector<std::string> textures;
	std::vector<LumenLight> lights;

	struct Dimensions {
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 max = glm::vec3(std::numeric_limits<float>::min());
		glm::vec3 size{0.f};
		glm::vec3 center{0.f};
		float radius{0};
	} m_dimensions;
	SceneConfig config;

   private:
	void compute_scene_dimensions();
};
