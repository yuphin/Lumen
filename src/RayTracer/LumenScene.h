#pragma once
#include "Framework/Camera.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"

#include "shaders/commons.h"
#include "Framework/MitsubaParser.h"
#include "SceneConfig.h"
#include "Framework/Buffer.h"
#include "Framework/Texture.h"
#include "Framework/Camera.h"

struct MeshData {
	std::vector<glm::vec3> positions;
	std::vector<u32> indices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec3> tangents;
	std::vector<glm::vec2> texcoords0;
	std::vector<glm::vec2> texcoords1;
	std::vector<glm::vec4> colors0;
};

struct LumenPrimMesh {
	std::string name;
	std::string filename;
	u32 material_idx;
	u32 vtx_offset;
	u32 first_idx;
	u32 idx_count;
	u32 vtx_count;
	u32 prim_idx;
	glm::mat4 world_matrix;
	glm::vec3 min_pos;
	glm::vec3 max_pos;
};

struct LumenLight {
	glm::vec3 pos;
	glm::vec3 to;
	glm::vec3 L;
	u32 light_flags;
	f32 world_radius;
	bool enabled = true;
};

struct LumenNode {
	std::string_view key = "";
	std::string_view value = "";
	LumenNode* parent = nullptr;
	LumenNode* child = nullptr;
	LumenNode* next = nullptr;
	i32 num_list_items = 0;
};

class LumenScene {
   public:
	LumenScene() = default;
	void load_scene(const std::string& path);
	void write_lumen_scene();
	void destroy();
	std::vector<glm::vec3> positions;
	std::vector<u32> indices;
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
	vk::Buffer* index_buffer;
	vk::Buffer* vertex_buffer;
	vk::Buffer* compact_vertices_buffer;
	vk::Buffer* materials_buffer;
	vk::Buffer* prim_lookup_buffer;
	vk::Buffer* scene_desc_buffer;
	vk::Buffer* mesh_lights_buffer;
	std::vector<vk::Texture*> scene_textures;
	std::unique_ptr<lm::Camera> camera;
	std::unordered_map<u32, std::string> material_idx_to_name;

	u32 total_light_triangle_cnt = 0;
	f32 total_light_area = 0;

	struct Dimensions {
		glm::vec3 min = glm::vec3(std::numeric_limits<f32>::max());
		glm::vec3 max = glm::vec3(std::numeric_limits<f32>::min());
		glm::vec3 size{0.f};
		glm::vec3 center{0.f};
		f32 radius{0};
	} m_dimensions;
	std::unique_ptr<SceneConfig> config;

	u32 dir_light_idx = -1;
	void create_scene_config(const std::string& integrator_name);
	inline bool has_bsdf_type(u32 flag) { return (bsdf_types & flag) != 0; }

   private:
	u32 bsdf_types = 0;
	void compute_scene_dimensions();
	void load_lumen_scene(const std::string& path);
	void load_mitsuba_scene(const std::string& path);
	void load_lumen_scene_new(const std::string& path);
	void parse_lumen_scene(const std::string& path, LumenNode* root);
	void add_default_texture();
	VkSampler texture_sampler;
};