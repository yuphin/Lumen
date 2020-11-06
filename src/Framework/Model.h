#pragma once
#include "LumenPCH.h"
#include <tiny_gltf.h>
#include "Texture.h"
#include "Buffer.h"
#include <functional>

class Model {

public:
	struct Primitive {
		uint32_t first_idx;
		uint32_t idx_cnt;
		int32_t material_idx;
	};

	struct Mesh {
		std::vector<Primitive> primitives;
	};

	struct Node {
		Node* parent = nullptr;
		std::vector<Node> children;
		Mesh mesh;
		glm::mat4 matrix;
		std::string name;
		bool visible = true;
	};
	struct Material {
		enum class AlphaMode { LMN_OPAQUE, LMN_MASK, LMN_BLEND };
		// These are either 0 or 1, corresponding to UV0 and UV1
		struct UVSets {
			uint8_t base_color = 0xFF;
			uint8_t normal = 0xFF;
			uint8_t metallic_roughness = 0xFF;
			uint8_t occlusion = 0xFF;
			uint8_t emissive = 0xFF;
			uint8_t specular_glossiness = 0xFF;
		};
		uint32_t base_color_idx;
		uint32_t normal_texture_idx;
		uint32_t metallic_roughness_idx;
		uint32_t occlusion_idx;
		uint32_t emissive_idx;
		uint32_t diffuse_idx;
		uint32_t specular_glossiness_idx;

		UVSets uv_sets = {};
		AlphaMode alpha_mode = AlphaMode::LMN_OPAQUE;

		glm::vec4 base_color_factor = glm::vec4(1.0f);
		glm::vec4 emisisive_factor = glm::vec4(1.0f);
		glm::vec4 diffuse_factor = glm::vec4(1.0f);
		glm::vec3 specular_factor = glm::vec3(0.0f);
		float roughness_factor = 1.0f;
		float metallic_factor = 1.0f;
		float glossiness_factor = 1.0f;

		float alpha_cutoff = 1.0f;

		bool metallic_roughness = true;
		bool specular_glossiness = false;
		bool double_sided = false;

		VkDescriptorSet descriptor_set;
		VkPipeline pipeline;
	};

public:

	using RenderFunction = std::function<void(Model::Material*, const VkCommandBuffer&,
											  const VkPipelineLayout&, size_t)>;
	struct Vertex {
		glm::vec2 uv0;
		glm::vec2 uv1;
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 color;
		glm::vec4 tangent;
	};
	Model() = default;
	Model(VulkanContext*);
	Model(VulkanContext* ctx, const std::string path, const std::string& name);
	void destroy(const VkDevice&);

	VkDescriptorImageInfo get_texture_descriptor(const size_t index);
	void load_materials(const tinygltf::Model& input);
	void load_textures(const tinygltf::Model& input);
	void load_node(const tinygltf::Node& input_node,
				   const tinygltf::Model& input,
				   Model::Node* parent,
				   std::vector<uint32_t>& idx_buffer,
				   std::vector<Model::Vertex>& vertex_buffer);
	void draw_node(const VkCommandBuffer& command_buffer,
				   const VkPipelineLayout& pipeline_layout,
				   const Model::Node& node,
				   size_t cb_index, RenderFunction);
	void draw(const VkCommandBuffer& command_buffer, const VkPipelineLayout& pipeline_layout,
			  size_t cb_index, RenderFunction = nullptr
	);
	inline void set_context(VulkanContext* ctx) { this->ctx = ctx; }

	std::string name;
	std::string path;
	// For storage in linear buffers
	Buffer vertices, indices;
	std::vector<Texture2D> textures;
	std::vector<Material> materials;
	std::vector<Node> nodes;
	VkDescriptorSet descriptor_set;
private:

	VulkanContext* ctx;
};
