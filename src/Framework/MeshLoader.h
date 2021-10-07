#pragma once
#include "LumenPCH.h"
#include "Model.h"

class MeshLoader {
public:
	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
		glm::vec4 tangent;
	};

	struct Indices {
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct Vertices {
		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	struct Mesh {
		std::vector<Primitive> primitives;
	};

	struct Node {
		Node* parent;
		std::vector<Node> children;
		Mesh mesh;
		glm::mat4 matrix;
		std::string name;
		bool visible = true;
	};

	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
		uint32_t normalTextureIndex;
		std::string alphaMode = "OPAQUE";
		float alphaCutOff;
		bool doubleSided = false;
		VkDescriptorSet descriptorSet;
		VkPipeline pipeline;
	};

	inline void init(VulkanContext* ctx) { this->ctx = ctx; };
	void add_model(Model* model);
	void create_cube_model(Model::Material& material, GLTFModel& cube_model);
	void load_gltf();
	void destroy();
	uint32_t num_materials;
	uint32_t num_textures;
	std::vector<Model*> models;
private:
	VulkanContext* ctx;
	bool load_gltf_helper(GLTFModel& model);

};
