#pragma once
#include "../LumenPCH.h"
#include <tiny_obj_loader.h>
#include "Framework/LumenInstance.h"
#include "shaders/commons.h"
#include "Framework/MitsubaParser.h"
#include "SceneConfig.h"
#include "Framework/Buffer.h"
#include "Framework/Texture.h"
#include "Framework/Camera.h"

struct MeshData {
	std::vector<glm::vec3> positions;
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> tangents;
	std::vector<glm::vec2> texcoords0;
	std::vector<glm::vec2> texcoords1;
	std::vector<glm::vec4> colors0;
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
	bool enabled = true;
};

class LumenScene {
   public:
   LumenScene(lumen::LumenInstance* instance) : instance(instance) {}
	void load_scene(const std::string& path);
	void destroy();
	std::vector<glm::vec3> positions;
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> tangents;
	std::vector<glm::vec2> texcoords0;
	std::vector<glm::vec2> texcoords1;
	std::vector<glm::vec4> colors0;

	std::vector<LumenPrimMesh> prim_meshes;
	std::vector<Material> materials;
	std::vector<std::string> textures;
	std::vector<LumenLight> lights;

	std::vector<Light> gpu_lights;
	lumen::BufferOld index_buffer;
	lumen::BufferOld vertex_buffer;
	lumen::BufferOld compact_vertices_buffer;
	lumen::BufferOld materials_buffer;
	lumen::BufferOld prim_lookup_buffer;
	lumen::BufferOld scene_desc_buffer;
	lumen::BufferOld mesh_lights_buffer;
	std::vector<lumen::Texture2D> scene_textures;
	std::unique_ptr<lumen::Camera> camera;

	uint32_t total_light_triangle_cnt = 0;
	float total_light_area = 0;

	struct Dimensions {
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 max = glm::vec3(std::numeric_limits<float>::min());
		glm::vec3 size{0.f};
		glm::vec3 center{0.f};
		float radius{0};
	} m_dimensions;
	std::unique_ptr<SceneConfig> config;

	uint32_t dir_light_idx = -1;
	void create_scene_config(const std::string& integrator_name);
	inline bool has_bsdf_type(uint32_t flag) { return (bsdf_types & flag) != 0; }

   private:
	uint32_t bsdf_types = 0;
	void compute_scene_dimensions();
	void load_lumen_scene(const std::string& path);
	void load_mitsuba_scene(const std::string& path);
	void add_default_texture();
	lumen::LumenInstance* instance;
	VkSampler texture_sampler;
};
