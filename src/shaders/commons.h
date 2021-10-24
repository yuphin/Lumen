#ifndef COMMONS_HOST_DEVICE
#define COMMONS_HOST_DEVICE

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using ivec3 = glm::ivec3;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#define ALIGN16 alignas(16)
#else
#define ALIGN16
#endif

struct PushConstantRay
{
	vec4 clear_color;
	vec3 light_pos;
	float light_intensity;
	int light_type;
	int num_mesh_lights;
	float total_light_area;
	uint frame_num;
};

struct SceneUBO {
	mat4 projection;
	mat4 view;
	mat4 model;
	mat4 inv_view;
	mat4 inv_projection;
	vec4 light_pos;
	vec4 view_pos;
};

struct Vertex {
	vec3 pos;
	vec3 normal;
	vec2 uv0;
};

struct MeshLight {
	mat4 world_matrix;
	uint prim_mesh_idx;
	uint num_triangles;
	uint pad0;
	uint pad1;
};

struct LightVisibility {
	float weight;
	float cdf;
};

struct GLTFMaterial {
	vec4 base_color_factor;
	vec3 emissive_factor;
	int texture_id;
};

// Scene buffer addresses
struct SceneDesc {
	uint64_t vertex_addr;
	uint64_t normal_addr;
	uint64_t uv_addr;
	uint64_t index_addr;
	uint64_t material_addr;
	uint64_t prim_info_addr;
	uint64_t mesh_lights_addr;
	uint64_t light_vis_addr;
};

// Structure used for retrieving the primitive information in the closest hit
struct PrimMeshInfo
{
	uint index_offset;
	uint vertex_offset;
	int  material_index;
};


#ifdef __cplusplus // Descriptor binding helper for C++ and GLSL
#define START_BINDING(a) enum a {
#define END_BINDING() }
#else
#define START_BINDING(a)  const uint
#define END_BINDING() 
#endif

#endif
