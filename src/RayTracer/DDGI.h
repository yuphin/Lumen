#pragma once
#include "shaders/integrators/ddgi/ddgi_commons.h"

struct Integrator;

struct DDGI {
	DDGIUniforms ddgi_ubo{};
	vk::Buffer* ddgi_ubo_buffer = nullptr;
	vk::Buffer* direct_lighting_buffer = nullptr;
	vk::Buffer* probe_offsets_buffer = nullptr;
	vk::Buffer* g_buffer = nullptr;
	vk::Texture* irr_texes[2] = {};
	vk::Texture* depth_texes[2] = {};
	vk::Buffer* ddgi_output_buffer = nullptr;
	struct {
		vk::Texture* radiance_tex = nullptr;
		vk::Texture* dir_depth_tex = nullptr;
	} rt;
	struct {
		vk::Texture* tex = nullptr;
	} output;
	f32 hysteresis = 0.98f;
	u32 rays_per_probe = 256;
	f32 depth_sharpness = 50.0f;
	f32 normal_bias = 0.6f;
	f32 backface_ratio = 0.1f;
	f32 probe_distance = 0.5f;
	f32 min_frontface_dist = 0.1f;
	f32 max_distance = 0;
	lm::ivec3 probe_counts{};
	lm::vec3 probe_start_position{};
	f32 tmax = 1e4f;
	f32 tmin = 1e-3f;
	PCDDGI pc{};
	VkSampler bilinear_sampler = VK_NULL_HANDLE;
	VkSampler nearest_sampler = VK_NULL_HANDLE;
	bool first_frame = true;
	bool infinite_bounces = true;
	bool direct_lighting = true;
	bool visualize_probes = false;
	u32 frame_idx = 0;
	u32 total_frame_idx = 0;
	vk::Buffer* sphere_vertices_buffer = nullptr;
	vk::Buffer* sphere_indices_buffer = nullptr;
	vk::Buffer* sphere_desc_buffer = nullptr;
	lm::Array<SphereVertex> sphere_vertices;
	lm::Array<u32> sphere_indices;
};

namespace ddgi {
void update_ddgi_uniforms(Integrator* integrator);
void create_radiance_textures(Integrator* integrator);
lm::vec3 probe_location(Integrator* integrator, u32 index);
lm::ivec3 probe_index_to_grid_coord(Integrator* integrator, u32 index);
lm::vec3 grid_coord_to_position(Integrator* integrator, const lm::ivec3& grid_coord);
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
bool gui(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
void create_accel(Integrator* integrator, vk::BVH* tlas, lm::Array<vk::BVH>* blases);
}  // namespace ddgi
