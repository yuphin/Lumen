#pragma once
#include "shaders/commons.h"
#include "SceneConfig.h"
#include "Framework/Camera.h"
#include "Framework/Base/String.h"
#include "Framework/Base/Memory.h"
#include "Framework/Base/HashMap.h"

namespace vk {
struct Buffer;
struct Texture;
}  // namespace vk

struct LumenPrimMesh {
	lm::String name;
	lm::String filename;
	u32 material_idx;
	u32 vtx_offset;
	u32 first_idx;
	u32 idx_count;
	u32 vtx_count;
	u32 prim_idx;
	lm::mat4 world_matrix;
	lm::vec3 min_pos;
	lm::vec3 max_pos;
};

struct AnalyticalLight {
	lm::vec3 pos;
	lm::vec3 to;
	lm::vec3 L;
	u32 light_flags;
	f32 world_radius;
	f32 inner_angle;
	f32 outer_angle;
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
	lm::String allocation_name;
	lm::String relative_path;
};

namespace scene {

struct Scene {
	lm::FixedArray<lm::vec3> positions;
	lm::FixedArray<u32> indices;
	lm::FixedArray<lm::vec3> normals;
	lm::FixedArray<lm::vec2> texcoords0;
	lm::FixedArray<LumenPrimMesh> prim_meshes;
	lm::FixedArray<Material> materials;
	lm::FixedArray<TextureRef> textures;
	lm::FixedArray<vk::Texture*> scene_textures;
	lm::FixedArray<AnalyticalLight> analytical_lights;
	lm::HashMap<u32, lm::String> material_idx_to_name{};
	lm::FixedArray<Light> gpu_lights;
	lm::FixedArray<LightTriangleCDF> light_triangle_cdf;
	lm::FixedArray<u32> emitter_light_indices;

	vk::Buffer* index_buffer;
	vk::Buffer* vertex_buffer;
	vk::Buffer* materials_buffer;
	vk::Buffer* prim_lookup_buffer;
	vk::Buffer* transformations_buffer;
	vk::Buffer* scene_desc_buffer;
	vk::Buffer* mesh_lights_buffer;
	vk::Buffer* light_triangle_cdf_buffer;
	vk::Buffer* emitter_light_indices_buffer;
	lm::Camera camera{};

	struct Dimensions {
		lm::vec3 min = lm::vec3(F32_MAX);
		lm::vec3 max = lm::vec3(F32_MIN);
		lm::vec3 size = lm::vec3(0.f);
		lm::vec3 center = lm::vec3(0.f);
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
