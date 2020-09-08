#pragma once

#include "gfx/vulkan/Base.h"
#include "gfx/Shader.h"
#include "gfx/vulkan/PipelineDefault.h"
#include "lmhpch.h"
#include "gfx/PerspectiveCamera.h"
#include "gfx/Texture.h"
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
	void create_gfx_pipeline() override;
	void build_command_buffers() override;
	void setup_vertex_descriptions();
	void prepare_render();
	void prepare_buffers();
	void prepare_descriptor_layouts();
	void prepare_descriptor_pool();
	void prepare_descriptor_sets();
	void update_buffers();
	std::vector<Shader> shaders;

	struct {
		VkPipelineVertexInputStateCreateInfo input_state;
		std::vector<VkVertexInputBindingDescription> binding_descriptions;
		std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
	} vertex_descriptions = {};

	struct {
		Buffer triangle;
		Buffer scene_ubo;
	} vertex_buffers;

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 tex_coord;
	};

	struct SceneUBO {
		alignas(16)	glm::mat4 projection;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 model;
	};
	std::unique_ptr<DefaultPipeline> demo_pipeline = nullptr;
	std::unique_ptr<Camera> cam = nullptr;
	std::unique_ptr<Texture2D> tri_texture = nullptr;
	VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;;
	std::vector<VkDescriptorSet> descriptor_sets{};



};
