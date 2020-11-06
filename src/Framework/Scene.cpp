#include "LumenPCH.h"
#include "Scene.h"
void Scene::add_model(const std::string& filename, const std::string& name) {
	models.emplace_back(ctx, filename, name);
}

void Scene::add_cube_model(Model::Material& material) {
	Model cube_model(this->ctx);
	Model::Node node;
	Model::Primitive primitive;


	std::vector<Model::Vertex> vert_buf = {
		{{0.0, 0.33}, glm::vec2(0.0f), {0, 1, 0}, glm::vec3(-1, 0, 0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 0
		{{0.0, 0.66}, glm::vec2(0.0f), {0, 1, 1}, glm::vec3(-1,0,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 1
		{{0.25, 0.33}, glm::vec2(0.0f), {0, 0, 0}, glm::vec3(-1,0,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 2
		{{0.25, 0.66}, glm::vec2(0.0f), {0, 0, 1}, glm::vec3(-1,0,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 3
		{{0.5, 0.33}, glm::vec2(0.0f), {1, 0, 0}, glm::vec3(1,0,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 4
		{{0.5, 0.66}, glm::vec2(0.0f), {1, 0, 1}, glm::vec3(1,0,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 5
		{{0.75, 0.33}, glm::vec2(0.0f), {1, 1, 0}, glm::vec3(1,0,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 6
		{{0.75, 0.66},  glm::vec2(0.0f), {1, 1, 1}, glm::vec3(1,0,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 7
		{{1.0, 0.33}, glm::vec2(0.0f), {0, 1, 0}, glm::vec3(0,1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 8
		{{1.0, 0.66}, glm::vec2(0.0f), {0, 1, 1}, glm::vec3(0,1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 9
		{{0.25, 0.0}, glm::vec2(0.0f), {0, 1, 0}, glm::vec3(0,0, -1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 10 
		{{0.50, 0.0}, glm::vec2(0.0f), {1, 1, 0}, glm::vec3(0,0, -1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 11
		{{0.25, 1.0}, glm::vec2(0.0f), {0, 1, 1}, glm::vec3(0,0, 1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 12 
		{{0.50, 1.0}, glm::vec2(0.0f), {1, 1, 1}, glm::vec3(0,0, 1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 13
		{{0.75, 0.66}, glm::vec2(0.0f), {1, 1, 1}, glm::vec3(0,1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 14
		{{0.75, 0.33}, glm::vec2(0.0f), {1, 1, 0}, glm::vec3(0,1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 15
		{{0.25, 0.66}, glm::vec2(0.0f), {0, 0, 1}, glm::vec3(0,-1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 16
		{{0.25, 0.66}, glm::vec2(0.0f), {0, 0, 1}, glm::vec3(0,0, 1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 17
		{{0.5, 0.66}, glm::vec2(0.0f), {1, 0, 1}, glm::vec3(0,0,1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 18
		{{0.5, 0.66}, glm::vec2(0.0f), {1, 0, 1}, glm::vec3(0,-1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 19
		{{0.5, 0.33}, glm::vec2(0.0f), {1, 0, 0}, glm::vec3(0,0,-1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 20
		{{0.5, 0.33}, glm::vec2(0.0f), {1, 0, 0}, glm::vec3(0,-1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 21
		{{0.25, 0.33}, glm::vec2(0.0f), {0, 0, 0}, glm::vec3(0,0,-1), glm::vec3(0.0f), glm::vec4(0.0f)}, // 22
		{{0.25, 0.33}, glm::vec2(0.0f), {0, 0, 0}, glm::vec3(0,-1,0), glm::vec3(0.0f), glm::vec4(0.0f)}, // 23
	};
	std::vector<uint32_t> idx_buf = {
		0, 1 ,2, //left
		2, 1, 3,
		4, 5, 6, //right
		6, 5, 7,
		15, 14, 8, //top
		8, 14, 9,
		17, 12,18,  //front
		18, 12, 13,
		10, 22, 11, //back
		11, 22, 20,
		16, 21, 23, // bottom
		21, 16, 19
	};
	primitive.first_idx = 0;
	primitive.idx_cnt = static_cast<uint32_t>(idx_buf.size());
	primitive.material_idx = 0;


	auto vertex_buf_size = vert_buf.size() * sizeof(Model::Vertex);
	auto idx_buf_size = idx_buf.size() * sizeof(uint32_t);
	cube_model.vertices.create(
		ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		vertex_buf_size,
		vert_buf.data(),
		true
	);
	cube_model.indices.create(
		ctx,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		idx_buf_size,
		idx_buf.data(),
		true
	);

	node.mesh.primitives = { primitive };
	node.matrix = glm::mat4(1.0f);
	node.name = "Cube";
	node.children = {};
	cube_model.nodes = { node };
	cube_model.materials = { material };
	models.push_back(cube_model);
}

void Scene::load_models() {
	for(auto& model : models) {
		LUMEN_ASSERT(load_gltf(model), "Error loading scene");
		num_materials += static_cast<uint32_t>(model.materials.size());
		num_textures += static_cast<uint32_t>(model.textures.size());
	}
}

void Scene::destroy() {
	for(auto& model : models) {
		model.destroy(*ctx->device);
	}
}

bool Scene::load_gltf(Model& model) {
	tinygltf::Model input;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;
	bool res = loader.LoadASCIIFromFile(&input, &err, &warn, model.path + "/" + model.name);
	if(!warn.empty()) {
		LUMEN_WARN(warn.c_str());
	}
	if(!err.empty()) {
		LUMEN_ERROR(err.c_str());
	}
	if(!res) {
		LUMEN_ERROR("Failed to load glTF", model.path.c_str());
	}
	model.load_textures(input);
	model.load_materials(input);
	std::vector<uint32_t> idx_buffer;
	std::vector<Model::Vertex> vertex_buffer;
	const auto& scene = input.scenes[0];
	for(auto i = 0; i < scene.nodes.size(); i++) {
		const auto& node = input.nodes[scene.nodes[i]];
		model.load_node(node, input, nullptr, idx_buffer, vertex_buffer);
	}

	auto vertex_buf_size = vertex_buffer.size() * sizeof(Model::Vertex);
	auto idx_buf_size = idx_buffer.size() * sizeof(uint32_t);
	auto submission = ThreadPool::submit([&] {
		model.vertices.create(
			ctx,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			vertex_buf_size,
			vertex_buffer.data(),
			true
		);
		model.indices.create(
			ctx,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			idx_buf_size,
			idx_buffer.data(),
			true
		);
	}
	);
	submission.get();
	return res;
}