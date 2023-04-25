#pragma once
#include "Integrator.h"
class DDGI : public Integrator {
   public:
	DDGI(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), DDGIConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	void update_ddgi_uniforms();

	DDGIUniforms ddgi_ubo;
	Buffer ddgi_ubo_buffer;
	Buffer direct_lighting_buffer;
	Buffer probe_offsets_buffer;

	Buffer g_buffer;

	Texture2D irr_texes[2];
	Texture2D depth_texes[2];
	Buffer ddgi_output_buffer;

	struct {
		Texture2D radiance_tex;
		Texture2D dir_depth_tex;
	} rt;

	struct {
		Texture2D tex;
	} output;

	float hysteresis = 0.98f;
	int rays_per_probe = 256;
	float depth_sharpness = 50.0f;
	float normal_bias = 0.1f;
	float view_bias = 0.1f;
	float backface_ratio = 0.1f;
	float probe_distance = 0.5f;
	float min_frontface_dist = 0.1f;
	float max_distance;
	glm::ivec3 probe_counts;
	glm::vec3 probe_start_position;

	PushConstantRay pc_ray{};
	VkSampler bilinear_sampler;
	VkSampler nearest_sampler;
	bool first_frame = true;
	uint32_t frame_idx = 0;
	uint total_frame_idx = 0;

	DDGIConfig* config;
};
