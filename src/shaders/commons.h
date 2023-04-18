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
#define BSDF_ALL                                                               \
    BSDF_SPECULAR | BSDF_TRANSMISSIVE | BSDF_REFLECTIVE | BSDF_OPAQUE |        \
        BSDF_LAMBERTIAN

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
using uint = unsigned int;
#define ALIGN16 alignas(16)
#else
#define ALIGN16
#endif

#define ENABLE_DISNEY 0
#define DIFFUSE_ONLY 0
#define DIFFUSE_AND_GLOSSY_ONLY 1

struct PushConstantRay {
    mat4 probe_rotation;
    vec4 clear_color;
    vec3 light_pos;
    int num_lights;
    vec3 min_bounds;
    int max_depth;
    vec3 max_bounds;
    float total_light_area;
    ivec3 grid_res;
    float radius;
    vec3 sky_col;
    uint frame_num;
    uint time;
    uint size_x;
    uint size_y;
    float ppm_base_radius;
    int use_vm;
    int use_vc;
    int light_triangle_count;
    int use_area_sampling;
    uint mutation_counter;
    uint light_rand_count;
    uint cam_rand_count;
    uint connection_rand_count;
    uint random_num;
    uint num_bootstrap_samples;
    float mutations_per_pixel;
    uint num_mlt_threads;
    uint total_frame_num;
    uint do_spatiotemporal;
    uint max_angle_samples;
    float world_radius;
    float pad;
    int first_frame;
	uint dir_light_idx;
};

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
};

struct DDGIUniforms {
    ivec3 probe_counts;
    float hysteresis;
    vec3 probe_start_position;
    float probe_step;
    int rays_per_probe;
    float max_distance;
    float depth_sharpness;
    float normal_bias;
    float view_bias;
    float backface_ratio;
    int irradiance_width;
    int irradiance_height;
    int depth_width;
    int depth_height;
    float min_frontface_dist;
    float pad;
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
	//uint enabled;
};

struct LightVisibility {
    float weight;
    float cdf;
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

struct PathVertex {
    vec3 dir;
    vec3 n_s;
    vec3 pos;
    vec2 uv;
    vec3 throughput;
    uint light_flags;
    uint light_idx;
    uint material_idx;
    uint delta;
    float area;
    float pdf_fwd;
    float pdf_rev;
};

struct MLTPathVertex {
    vec3 dir;
    uint delta;
    vec3 n_s;
    float area;
    vec3 pos;
    uint light_idx;
    vec2 uv;
    uint light_flags;
    uint material_idx;
    vec3 throughput;
    float pdf_rev;
    uint coords;
    float pdf_fwd;
    vec2 pad;
};

struct VCMVertex {
    vec3 wi;
    vec3 wo;
    vec3 n_s;
    vec3 pos;
    vec2 uv;
    vec3 throughput;
    uint material_idx;
    uint path_len;
    float area;
    float d_vcm;
    float d_vc;
    float d_vm;
    uint side;
    uint coords;
};

struct SPPMData {
    vec3 p;
    vec3 wo;
    vec3 tau;
    vec3 col;
    vec3 phi;
    vec3 throughput;
    uint material_idx;
    vec2 uv;
    vec3 n_s;
    int M;
    float N;
    float radius;
    int path_len;
};

struct PhotonHash {
    vec3 pos;
    float d_vm;
    vec3 wi;
    float d_vcm;
    vec3 throughput;
    int photon_count;
    vec3 nrm;
    uint path_len;
};

struct Bounds {
    vec3 min_bnds;
    vec3 max_bnds;
};

struct HashData {
    uint hash_idx;
    uint pixel_idx;
};

struct AtomicData {
    vec3 min_bnds;
    vec3 max_bnds;
    ivec3 grid_res;
    float max_radius;
};

struct VertexBackup {
    float pdf_fwd;
    float pdf_rev;
};

struct PrimarySample {
    float val;
    float backup;
    uint last_modified;
    uint last_modified_backup;
};

struct MLTSampler {
    uint last_large_step;
    uint iter;
    // uint sampler_idx;
    uint num_light_samples;
    uint num_cam_samples;
    uint num_connection_samples;
    float luminance;
    uint splat_cnt;
    uint past_splat_cnt;
    uint swap;
    uint type;
};

struct VCMMLTSampler {
    uint last_large_step;
    uint iter;
    uint num_light_samples;
    float luminance;
    uint splat_cnt;
    uint past_splat_cnt;
    uint swap;
};

struct SeedData {
    uvec4 chain_seed;
    int depth;
};

struct VCMMLTSeedData {
    uvec4 chain0_seed;
    uvec4 chain1_seed;
};

struct ChainData {
    float total_luminance;
    uint lum_samples;
    float total_samples;
    float normalization;
};

struct SumData {
    float x; // lum accum
    uint y; // lum sample
    float z;
};

struct Splat {
    uint idx;
    vec3 L;
};

struct BootstrapSample {
    uvec4 seed;
    float lum;
};

struct ReservoirSample {
    vec3 x_v;
    float p_q;
    vec3 n_v;
    uint bsdf_props;
    vec3 x_s;
    uint mat_idx;
    vec3 n_s;
    vec3 L_o;
    vec3 f;
};

struct Reservoir {
    float w_sum;
    float W;
    uint m;
    uint pad;
    ReservoirSample s;
};

struct ReSTIRPTReservoir {
    float m;
    float W;
    vec3 L;
    uint path_flags;
    uvec4 init_seed;
    // Reconnection vertex data
    uvec4 rc_seed;
    vec3 rc_pos;
    vec3 rc_postfix_L;

};

struct GBufferData {
    vec3 pos;
    uint mat_idx;
    vec3 normal;
    uint pad;
    vec2 uv;
    vec2 pad2;
    vec3 albedo;
    uint pad3;
    vec3 n_g;
    uint pad4;
};

struct RestirData {
    uint light_idx;
    uint light_mesh_idx;
    uvec4 seed;
};

struct RestirReservoir {
    float w_sum;
    float W;
    uint m;
    RestirData s;
    float p_hat;
    float pdf;
};



struct VCMRestirData {
    uvec4 seed;
    vec3 pos;
    float p_hat;
    vec3 dir;
    float pdf_posdir;
    vec3 normal;
    float pdf_pos;
    float pdf_dir;
    float triangle_pdf;
    uint light_material_idx;
    uint hash_idx;
    uint valid;
    uint frame_idx;
    float phi;
    uint pad;
};

struct VCMReservoir {
    float w_sum;
    float W;
    uint m;
    uint sample_idx;
    uint selected_idx;
    uint factor;
    VCMRestirData s;
};

struct SelectedReservoirs {
    uint selected;
    vec3 pos;
    vec3 dir;
};

struct LightState {
    vec3 pos;
    float triangle_pdf;
    vec3 dir;
    uint hash_idx;
    vec3 normal;
    vec3 Le;
    uint light_flags;
};

struct AngleStruct {
    float phi;
    float theta;
    int is_active;
};

struct AvgStruct {
    float avg;
    uint prev;
};



// Scene buffer addresses
struct SceneDesc {
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
};

struct Desc2 {
    uint64_t test_addr;

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

#ifdef __cplusplus // Descriptor binding helper for C++ and GLSL
#define START_BINDING(a) enum a {
#define END_BINDING() }
#else
#define START_BINDING(a) const uint
#define END_BINDING()
#endif

#endif
