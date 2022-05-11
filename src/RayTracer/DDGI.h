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
	void update_ddgi_uniforms();

	DDGIUniforms ddgi_ubo;
	Buffer ddgi_ubo_buffer;
	Buffer direct_lighting_buffer;

	Buffer g_buffer;


	Texture2D irr_texes[2];
	Texture2D depth_texes[2];
	Buffer ddgi_output_buffer;

	struct {
		Texture2D radiance_tex;
		Texture2D dir_depth_tex;
		VkDescriptorSetLayout write_desc_layout = 0;
		VkDescriptorSetLayout read_desc_layout = 0;
		VkDescriptorSet write_desc_set = 0;
		VkDescriptorSet read_desc_set = 0;
	} rt;

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

	struct {
		VkDescriptorSet desc_set = 0;
		VkDescriptorSetLayout desc_layout = 0;
	} ddgi_uniform;

	float hysteresis = 0.98f;
	int rays_per_probe = 300;
	float depth_sharpness = 50.0f;
	float normal_bias = 0.1;
	float view_bias = 0.1;
	float probe_distance;
	float max_distance;
	glm::ivec3 probe_counts;
	glm::vec3 probe_start_position;

	PushConstantRay pc_ray{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout scene_desc_set_layout;
	VkDescriptorSet scene_desc_set;
	VkSampler bilinear_sampler;
	VkSampler nearest_sampler;
	std::unique_ptr<Pipeline> vis_pipeline;
	std::unique_ptr<Pipeline> probe_trace_pipeline;
	std::unique_ptr<Pipeline> depth_update_pipeline;
	std::unique_ptr<Pipeline> irr_update_pipeline;
	std::unique_ptr<Pipeline> border_update_pipeline;
	std::unique_ptr<Pipeline> sample_pipeline;
	std::unique_ptr<Pipeline> out_pipeline;
	bool first_frame = true;
	uint32_t frame_idx = 0;
};
