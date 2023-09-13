#ifndef COMMONS_HOST_DEVICE
#define COMMONS_HOST_DEVICE

#define INTEGRATOR_PT 0
#define INTEGRATOR_BDPT 1
#define INTEGRATOR_PPM_LIGHT 2
#define INTEGRATOR_PPM_EYE 3
#define INTEGRATOR_VCM_LIGHT 4
#define INTEGRATOR_VCM_EYE 5
#define INTERATOR_COUNT 6

// BSDF Types
#define BSDF_DIFFUSE 1 << 0
#define BSDF_MIRROR 1 << 1
#define BSDF_GLASS 1 << 2
#define BSDF_GLOSSY 1 << 3
#define BSDF_DISNEY 1 << 4
#define BSDF_NONE -1

// BSDF Props
#define BSDF_LAMBERTIAN 1 << 4
#define BSDF_SPECULAR 1 << 5
#define BSDF_TRANSMISSIVE 1 << 6
#define BSDF_REFLECTIVE 1 << 7
#define BSDF_OPAQUE 1 << 8
#define BSDF_ALL BSDF_SPECULAR | BSDF_TRANSMISSIVE | BSDF_REFLECTIVE | BSDF_OPAQUE | BSDF_LAMBERTIAN

// Light Type
#define LIGHT_SPOT 1
#define LIGHT_AREA 2
#define LIGHT_DIRECTIONAL 3

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using ivec3 = glm::ivec3;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4; 
using uvec4 = glm::uvec4;
using ivec2 = glm::ivec2;
using uint = unsigned int;
using uvec2 = glm::uvec2;
#define ALIGN16 alignas(16)
#define NAMESPACE_BEGIN(name) namespace name {
#define NAMESPACE_END() }
#else
#define NAMESPACE_BEGIN(name)
#define NAMESPACE_END()
#define ALIGN16
#endif


// Debug logger for Raygen shaders
#ifndef __cplusplus

	#define LOG_CLICKED0(str) \
		if(ubo.debug_click == 1 && ivec2(gl_LaunchIDEXT.xy) == ubo.clicked_pos) { \
			debugPrintfEXT(str); \
	}

	#define LOG_CLICKED(str, args) \
		if(ubo.debug_click == 1 && ivec2(gl_LaunchIDEXT.xy) == ubo.clicked_pos) { \
			debugPrintfEXT(str, args); \
		}

	#define LOG_CLICKED2(str, args1, args2) \
		if(ubo.debug_click == 1 && ivec2(gl_LaunchIDEXT.xy) == ubo.clicked_pos) { \
			debugPrintfEXT(str, args1, args2); \
		}

	#define LOG_CLICKED3(str, args1, args2, args3) \
		if(ubo.debug_click == 1 && ivec2(gl_LaunchIDEXT.xy) == ubo.clicked_pos) { \
			debugPrintfEXT(str, args1, args2, args3); \
		}

	#define LOG_CLICKED4(str, args1, args2, args3, args4) \
		if(ubo.debug_click == 1 && ivec2(gl_LaunchIDEXT.xy) == ubo.clicked_pos) { \
			debugPrintfEXT(str, args1, args2, args3, args4); \
		}

	#define ASSERT_CLICKED_STR(cond, expected, str, val) \
		if(ubo.debug_click == 1 && ivec2(gl_LaunchIDEXT.xy) == ubo.clicked_pos) { \
			if(cond != expected)  {\
				debugPrintfEXT("Assertion failed: "); \
				debugPrintfEXT(str, val); \
			} \
		}

	#define ASSERT_CLICKED(cond, expected) \
		if(ubo.debug_click == 1 && ivec2(gl_LaunchIDEXT.xy) == ubo.clicked_pos) { \
			if(cond != expected)  {\
				debugPrintfEXT("Assertion failed!\n"); \
			} \
		}

	#define LOG_VAL(str, args, coord) \
		if(ivec2(gl_LaunchIDEXT.xy) == coord) { \
			debugPrintfEXT(str, args); \
		}

	#define LOG(str, coord) \
		if(ivec2(gl_LaunchIDEXT.xy) == coord) { \
			debugPrintfEXT(str); \
		}

	#define LOG0(str) debugPrintfEXT(str);
	#define LOG1(str, val) debugPrintfEXT(str, val);
	#define LOG2(str, val, val2) debugPrintfEXT(str, val, val2);
	#define LOG3(str, val, val2, val3) debugPrintfEXT(str, val, val2, val3);

	#define ASSERT(cond) \
		if(!(cond))  { \
			debugPrintfEXT("Assertion failed on pixel %v2i\n", ivec2(gl_LaunchIDEXT.xy)); \
		}

	#define ASSERT0(cond, str) \
		if(!(cond))  { \
			debugPrintfEXT(str); \
		}

	#define ASSERT1(cond, str, val1) \
		if(!(cond))  { \
			debugPrintfEXT(str, val1); \
		}
		

#endif

#define ENABLE_DISNEY 0
#define DIFFUSE_ONLY 0
#define DIFFUSE_AND_GLOSSY_ONLY 1





struct PCPost {
	uint enable_tonemapping;
	uint enable_bloom;
	int width;
	int height;
	float bloom_exposure;
	float bloom_amount;
};

struct PushConstantCompute {
	uint num_elems;
	uint base_idx;
	uint block_idx;
	uint n;
	uint store_sum;
	uint scan_sums;
	uint64_t block_sum_addr;
	uint64_t out_addr;
};

struct SceneUBO {
	mat4 projection;
	mat4 view;
	mat4 model;
	mat4 inv_view;
	mat4 inv_projection;
	vec4 light_pos;
	vec4 view_pos;
	mat4 prev_view;
	mat4 prev_projection;
	ivec2 clicked_pos;
	int debug_click;
};

struct Vertex {
	vec3 pos;
	vec3 normal;
	vec2 uv0;
};

struct Light {
	mat4 world_matrix;
	vec3 pos;
	uint prim_mesh_idx;
	vec3 to;
	uint num_triangles;
	vec3 L;
	uint light_flags;
	vec3 world_center;
	float world_radius;
};

struct Material {
	vec3 albedo;
	vec3 emissive_factor;
	float ior;
	uint bsdf_type;
	uint bsdf_props;
	int texture_id;
	vec3 metalness;
	float roughness;
	// Disney BSDF
	float metallic;
	float specular_tint;
	float sheen_tint;
	float specular;
	float clearcoat;
	float clearcoat_gloss;
	float sheen;
	float subsurface;
};

// Scene buffer addresses
 struct ALIGN16 SceneDesc {
	uint64_t vertex_addr;
	uint64_t normal_addr;
	uint64_t uv_addr;
	uint64_t index_addr;
	uint64_t material_addr;
	uint64_t prim_info_addr;
	// NEE
	uint64_t mesh_lights_addr;
	uint64_t light_vis_addr;
	// BDPT
	uint64_t light_path_addr;
	uint64_t camera_path_addr;
	uint64_t path_backup_addr;
	uint64_t color_storage_addr;
	// SPPM
	uint64_t sppm_data_addr;
	uint64_t residual_addr;
	uint64_t counter_addr;
	uint64_t atomic_data_addr;
	uint64_t hash_addr;
	uint64_t photon_addr;
	uint64_t tmp_col_addr;
	// VCM
	uint64_t vcm_vertices_addr;
	uint64_t path_cnt_addr;
	// MLT
	uint64_t bootstrap_addr;
	uint64_t cdf_addr;
	uint64_t cdf_sum_addr;
	uint64_t seeds_addr;
	uint64_t mlt_samplers_addr;
	uint64_t light_primary_samples_addr;
	uint64_t cam_primary_samples_addr;
	uint64_t connection_primary_samples_addr;
	uint64_t mlt_col_addr;
	uint64_t chain_stats_addr;
	uint64_t splat_addr;
	uint64_t past_splat_addr;
	uint64_t block_sums_addr;

	uint64_t connected_lights_addr;
	uint64_t tmp_seeds_addr;
	uint64_t tmp_lum_addr;
	uint64_t prob_carryover_addr;
	uint64_t light_splats_addr;
	uint64_t light_splat_cnts_addr;
	uint64_t mlt_atomicsum_addr;
	// ReSTIR
	uint64_t restir_samples_addr;
	uint64_t restir_samples_old_addr;
	uint64_t g_buffer_addr;
	uint64_t temporal_reservoir_addr;
	uint64_t passthrough_reservoir_addr;
	uint64_t spatial_reservoir_addr;
	// ReSTIR PT
	uint64_t gris_gbuffer_addr;
	uint64_t gris_reservoir_addr;
	uint64_t gris_direct_lighting_addr;
	uint64_t prefix_contributions_addr;
	uint64_t transformations_addr;
	uint64_t compact_vertices_addr;

	// VCM Reservoir
	uint64_t vcm_reservoir_addr;
	uint64_t selected_reservoirs_addr;
	uint64_t light_triangle_bins_addr;
	uint64_t sorted_bins_addr;
	uint64_t light_samples_addr;
	uint64_t should_resample_addr;
	uint64_t light_state_addr;
	uint64_t angle_struct_addr;
	uint64_t avg_addr;
	// DDGI
	uint64_t probe_radiance_addr;
	uint64_t probe_dir_depth_addr;
	uint64_t direct_lighting_addr;
	uint64_t probe_offsets_addr;
	// NRC
	uint64_t radiance_query_addr;
	uint64_t radiance_target_addr;
	uint64_t radiance_query_out_addr;
	uint64_t radiance_target_out_addr;
	uint64_t sample_count_addr;
	uint64_t inference_radiance_addr;
	uint64_t inference_query_addr;
	uint64_t throughput_addr;
 };


struct RTUtilsDesc {
	uint64_t out_img_addr;
	uint64_t gt_img_addr;
	uint64_t residual_addr;
	uint64_t counter_addr;
	uint64_t rmse_val_addr;
};

struct RTUtilsPC {
	uint size;
};

struct FFTPC {
	uint idx;
	uint n;
};

// Structure used for retrieving the primitive information in the closest hit
struct PrimMeshInfo {
	uint index_offset;
	uint vertex_offset;
	uint material_index;
	uint pad;
	vec4 min_pos;
	vec4 max_pos;
};

#endif
