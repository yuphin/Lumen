#pragma once
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"

#include "shaders/commons.h"
#include "SceneConfig.h"
#include "Framework/Buffer.h"
#include "Framework/Texture.h"
#include "Framework/Camera.h"
#include "Framework/Base/String.h"
#include "Framework/Base/OS.h"
#include "Framework/Base/Memory.h"
#include "Framework/Base/HashMap.h"

struct LumenPrimMesh {
	lm::String name;
	lm::String filename;
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
	lm::String key = "";
	lm::String value = "";
	LumenNode* parent = nullptr;
	LumenNode* child = nullptr;
	LumenNode* next = nullptr;
	i32 num_list_items = 0;
};

struct TextureRef {
	lm::String name;
	lm::String relative_path;
};

namespace scene {

struct Scene {
	lm::FixedArray<glm::vec3> positions;
	lm::FixedArray<u32> indices;
	lm::FixedArray<glm::vec3> normals;
	lm::FixedArray<glm::vec3> tangents;
	lm::FixedArray<glm::vec2> texcoords0;
	lm::FixedArray<glm::vec2> texcoords1;
	lm::FixedArray<glm::vec4> colors0;
	lm::FixedArray<LumenPrimMesh> prim_meshes;
	lm::FixedArray<Material> materials;
	lm::FixedArray<TextureRef> textures;
	lm::FixedArray<LumenLight> lights;
	lm::FixedArray<Light> gpu_lights;
	lm::FixedArray<vk::Texture*> scene_textures;

	vk::Buffer* index_buffer;
	vk::Buffer* vertex_buffer;
	vk::Buffer* compact_vertices_buffer;
	vk::Buffer* materials_buffer;
	vk::Buffer* prim_lookup_buffer;
	vk::Buffer* scene_desc_buffer;
	vk::Buffer* mesh_lights_buffer;
	lm::Camera camera{};
	lm::HashMap<u32, lm::String> material_idx_to_name{};

	u32 total_light_triangle_cnt = 0;
	f32 total_light_area = 0;

	struct Dimensions {
		glm::vec3 min = glm::vec3(F32_MAX);
		glm::vec3 max = glm::vec3(F32_MIN);
		glm::vec3 size = glm::vec3(0.f);
		glm::vec3 center = glm::vec3(0.f);
		f32 radius = 0.0f;
	} dimensions;
	SceneConfig config;

	u32 dir_light_idx = U32_MAX;
	u32 bsdf_types = 0;
	VkSampler scene_texture_sampler;
};

void load(const lm::String& path);
void write();
void destroy();
void config_init(const lm::String& integrator_name, const SceneCommon& common_config,
				 LumenNode* integrator_node = nullptr);
Scene* get();
}  // namespace scene