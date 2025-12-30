#pragma once
#include "Integrator.h"
#include "shaders/integrators/ddgi/ddgi_commons.h"
class DDGI final : public Integrator {
   public:
	DDGI(const vk::BVH& tlas) : Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual bool gui() override;
	virtual void destroy(bool resize) override;
	virtual void create_accel(vk::BVH& tlas, lm::Array<vk::BVH>& blases) override;

   private:
	void update_ddgi_uniforms();
	void create_radiance_textures();

	glm::vec3 probe_location(u32 index);
	glm::ivec3 probe_index_to_grid_coord(u32 index);
	glm::vec3 grid_coord_to_position(const glm::ivec3& grid_coord);

	DDGIUniforms ddgi_ubo;
	vk::Buffer* ddgi_ubo_buffer;
	vk::Buffer* direct_lighting_buffer;
	vk::Buffer* probe_offsets_buffer;
	vk::Buffer* g_buffer;

	vk::Texture* irr_texes[2];
	vk::Texture* depth_texes[2];
	vk::Buffer* ddgi_output_buffer;

	struct {
		vk::Texture* radiance_tex = nullptr;
		vk::Texture* dir_depth_tex = nullptr;
	} rt;

	struct {
		vk::Texture* tex;
	} output;

	f32 hysteresis = 0.98f;
	u32 rays_per_probe = 256;
	f32 depth_sharpness = 50.0f;
	f32 normal_bias = 0.6f;
	f32 backface_ratio = 0.1f;
	f32 probe_distance = 0.5f;
	f32 min_frontface_dist = 0.1f;
	f32 max_distance;
	glm::ivec3 probe_counts;
	glm::vec3 probe_start_position;
	f32 tmax = 1e4f;
	f32 tmin = 1e-3f;
	PCDDGI pc_ray{};
	VkSampler bilinear_sampler;
	VkSampler nearest_sampler;
	bool first_frame = true;
	bool infinite_bounces = true;
	bool direct_lighting = true;
	bool visualize_probes = false;
	u32 frame_idx = 0;
	uint total_frame_idx = 0;

	vk::Buffer* sphere_vertices_buffer;
	vk::Buffer* sphere_indices_buffer;
	vk::Buffer* sphere_desc_buffer;

	std::vector<SphereVertex> sphere_vertices;
	std::vector<u32> sphere_indices;
};
