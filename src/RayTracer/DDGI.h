#pragma once
#include "Integrator.h"
#include "shaders/integrators/ddgi/ddgi_commons.h"
class DDGI : public Integrator {
   public:
	DDGI(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), DDGIConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual bool gui() override;
	virtual void destroy() override;

   private:
	void update_ddgi_uniforms();
	void create_radiance_textures();

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

	float hysteresis = 0.98f;
	uint32_t rays_per_probe = 256;
	float depth_sharpness = 50.0f;
	float normal_bias = 0.6f;
	float backface_ratio = 0.1f;
	float probe_distance = 0.5f;
	float min_frontface_dist = 0.1f;
	float max_distance;
	glm::ivec3 probe_counts;
	glm::vec3 probe_start_position;
	float tmax = 1e4f;
	float tmin = 1e-3f;
	PCDDGI pc_ray{};
	VkSampler bilinear_sampler;
	VkSampler nearest_sampler;
	bool first_frame = true;
	uint32_t frame_idx = 0;
	uint total_frame_idx = 0;

	DDGIConfig* config;
};
