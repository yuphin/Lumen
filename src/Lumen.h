#pragma once

#include "LumenPCH.h"
#include "Framework/VulkanBase.h"
#include "Framework/Shader.h"
#include "Framework/Pipeline.h"
#include "Framework/Camera.h"
#include "Framework/Texture.h"
#include "Framework/Scene.h"
#include <glm/glm.hpp>

class Lumen : public VulkanBase {
public:
	Lumen(int width, int height, bool debug);
	void init(GLFWwindow*);
	void update();
	~Lumen();
	int width, height, fullscreen, debug;
	static Lumen* instance;
	inline static Lumen* get() { return instance; }
private:
	void create_render_pass() override;
	void build_command_buffers() override;
	void prepare_buffers();
	void prepare_descriptors();
	void prepare_descriptor_pool();
	void update_buffers();
	void init_resources();
	std::string get_asset_path() const;
	struct SceneUBO {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model = glm::mat4(1.0f);
		glm::vec4 light_pos = glm::vec4(3.0f, 2.5f, 1.0f, 1.0f);
		glm::vec4 view_pos;
	};

	struct SceneResources {
		struct {
			Buffer scene_ubo;
		} vertex_buffers = {};

		struct MaterialPushConst {
			glm::vec4 base_color_factor;
			int base_color_set;
		} material_push_const;
		Model::Material cube_material;
		GraphicsPipelineSettings cube_pipeline_settings = {};
		VkDescriptorSetLayout uniform_set_layout = VK_NULL_HANDLE;
		VkDescriptorSetLayout scene_set_layout = VK_NULL_HANDLE;
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> uniform_descriptor_sets{};
		std::unique_ptr<Pipeline> cube_pipeline = nullptr;
		Model::RenderFunction material_render_func = nullptr;
	} resources;
	std::unique_ptr<Camera> camera = nullptr;
	Scene scene;
	VulkanContext lumen_ctx;
};
