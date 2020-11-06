#pragma once
#include "LumenPCH.h"
#include "Model.h"

class Scene {
public:
	inline void init(VulkanContext* ctx) { this->ctx = ctx; };
	void add_model(const std::string& filename, const std::string& name);
	void add_cube_model(Model::Material& material);
	void load_models();
	void destroy();
	uint32_t num_materials;
	uint32_t num_textures;
	std::vector<Model> models;
private:
	VulkanContext* ctx;
	bool load_gltf(Model& model);

};
