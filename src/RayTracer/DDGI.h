#pragma once
#include "Integrator.h"
class DDGI : public Integrator {
public:
	DDGI(LumenInstance* scene, LumenScene* lumen_scene) :
		Integrator(scene, lumen_scene) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
	virtual void reload() override;
private:
	void create_offscreen_resources();
	void create_descriptors();
	void create_blas();
	void create_tlas();
	void create_rt_pipelines();
	void create_compute_pipelines();

	DDGIUniforms ubo;
	Buffer ddgi_ubo_buffer;
	Buffer g_buffer;
	Buffer probe_radiance_buffer;
	Buffer probe_dir_depth_buffer;
	Buffer ddgi_output_buffer;
	Texture2D irr_texes[2];
	Texture2D depth_texes[2];

	struct {
		VkDescriptorSetLayout write_desc_layout = 0;
		VkDescriptorSetLayout read_desc_layout = 0;
		VkDescriptorSet write_desc_sets[2];
		VkDescriptorSet read_desc_sets[2];
	} ddgi;

	struct {
		VkDescriptorSetLayout write_desc_layout = 0;
		VkDescriptorSetLayout read_desc_layout = 0;
		VkDescriptorSet write_desc_set = 0;
		VkDescriptorSet read_desc_set = 0;
		Texture2D tex;
	} output;

	float probe_distance;
	float max_distance;
	float hysteresis = 0.98;
	int rays_per_probe = 256;
	float depth_sharpness = 3.0f;
	float normal_bias = 0.25f;
	glm::ivec3 probe_counts;
	glm::vec3 probe_start_position;

	PushConstantRay pc_ray{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;
	VkSampler bilinear_sampler;
	VkSampler nearest_sampler;
	std::unique_ptr<Pipeline> rt_pipeline;
	bool first_frame = true;
};
