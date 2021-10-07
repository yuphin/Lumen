#include "LumenPCH.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Model.h"
#include "Framework/Utils.h"


GLTFModel::GLTFModel(const std::string path, const std::string& name) :
	Model(path, name) {}

VkDescriptorImageInfo GLTFModel::get_texture_descriptor(const size_t index) {
	return textures[index].descriptor_image_info;
}

void GLTFModel::load_materials(const tinygltf::Model& input) {
	materials.resize(input.materials.size());
	for(size_t i = 0; i < input.materials.size(); i++) {
		tinygltf::Material gltf_material = input.materials[i];
		if(gltf_material.values.find("baseColorFactor") != gltf_material.values.end()) {
			materials[i].base_color_factor = glm::make_vec4(gltf_material.values["baseColorFactor"].ColorFactor().data());
		}
		if(gltf_material.values.find("emissiveFactor") != gltf_material.values.end()) {
			materials[i].emisisive_factor =
				glm::vec4(glm::make_vec3(gltf_material.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
		}
		if(gltf_material.values.find("roughnessFactor") != gltf_material.values.end()) {
			materials[i].roughness_factor = static_cast<float>(gltf_material.values["roughnessFactor"].Factor());
		}
		if(gltf_material.values.find("metallicFactor") != gltf_material.values.end()) {
			materials[i].metallic_factor = static_cast<float>(gltf_material.values["metallicFactor"].Factor());
		}

		// SpecularGlossiness and diffuse factor comes with extensions
		if(gltf_material.extensions.find("KHR_materials_pbrSpecularGlossiness") != gltf_material.extensions.end()) {
			auto ext = gltf_material.extensions.find("KHR_materials_pbrSpecularGlossiness");
			if(ext->second.Has("specularGlossinessTexture")) {
				auto spec_gloss_index = ext->second.Get("specularGlossinessTexture").Get("index");
				materials[i].specular_glossiness_idx = spec_gloss_index.Get<int>();
				auto spec_gloss_uv_set = ext->second.Get("specularGlossinessTexture").Get("texCoord");
				materials[i].uv_sets.specular_glossiness = spec_gloss_uv_set.Get<int>();
				materials[i].specular_glossiness = true;
			}
			if(ext->second.Has("diffuseTexture")) {
				auto diffuse_idx = ext->second.Get("diffuseTexture").Get("index");
				materials[i].diffuse_idx = diffuse_idx.Get<int>();

			}
			if(ext->second.Has("diffuseFactor")) {
				auto factor = ext->second.Get("diffuseFactor");
				for(uint32_t j = 0; j < factor.ArrayLen(); j++) {
					auto val = factor.Get(j);
					materials[i].diffuse_factor[j] = val.IsNumber() ? (float) val.Get<double>() : (float) val.Get<int>();
				}
			}
			if(ext->second.Has("specularFactor")) {
				auto factor = ext->second.Get("specularFactor");
				for(uint32_t j = 0; j < factor.ArrayLen(); j++) {
					auto val = factor.Get(j);
					materials[i].specular_factor[j] = val.IsNumber() ? (float) val.Get<double>() : (float) val.Get<int>();
				}
			}
			if(ext->second.Has("glossinessFactor")) {
				auto factor = ext->second.Get("glossinessFactor");
				materials[i].glossiness_factor = factor.IsNumber() ? (float) factor.Get<double>() : (float) factor.Get<int>();
			}
		}

		if(gltf_material.values.find("baseColorTexture") != gltf_material.values.end()) {
			materials[i].base_color_idx = gltf_material.values["baseColorTexture"].TextureIndex();
			materials[i].uv_sets.base_color = gltf_material.values["baseColorTexture"].TextureTexCoord();
		}
		if(gltf_material.additionalValues.find("normalTexture") != gltf_material.additionalValues.end()) {
			materials[i].normal_texture_idx = gltf_material.additionalValues["normalTexture"].TextureIndex();
			materials[i].uv_sets.normal = gltf_material.values["normalTexture"].TextureTexCoord();
		}
		if(gltf_material.additionalValues.find("metallicRoughnessTexture") != gltf_material.additionalValues.end()) {
			materials[i].metallic_roughness_idx = gltf_material.additionalValues["metallicRoughnessTexture"].TextureIndex();
			materials[i].uv_sets.metallic_roughness = gltf_material.values["metallicRoughnessTexture"].TextureTexCoord();
			materials[i].metallic_roughness = true;
		}
		if(gltf_material.additionalValues.find("occlusionTexture") != gltf_material.additionalValues.end()) {
			materials[i].occlusion_idx = gltf_material.additionalValues["occlusionTexture"].TextureIndex();
			materials[i].uv_sets.occlusion = gltf_material.values["occlusionTexture"].TextureTexCoord();
		}
		if(gltf_material.additionalValues.find("emissiveTexture") != gltf_material.additionalValues.end()) {
			materials[i].emissive_idx = gltf_material.additionalValues["emissiveTexture"].TextureIndex();
			materials[i].uv_sets.emissive = gltf_material.values["emissiveTexture"].TextureTexCoord();
		}

		if(gltf_material.alphaMode == "OPAQUE") {
			materials[i].alpha_mode = Material::AlphaMode::LMN_OPAQUE;
		} else if(gltf_material.alphaMode == "MASK") {
			materials[i].alpha_mode = Material::AlphaMode::LMN_MASK;
		} else if(gltf_material.alphaMode == "BLEND") {
			materials[i].alpha_mode = Material::AlphaMode::LMN_BLEND;
		}
		materials[i].alpha_cutoff = (float) gltf_material.alphaCutoff;
		materials[i].double_sided = gltf_material.doubleSided;
	}
}

void GLTFModel::load_textures(VulkanContext *ctx, const tinygltf::Model& input) {
	textures = std::vector(input.images.size(), Texture2D(ctx));
	for(auto i = 0; i < input.images.size(); i++) {
		auto& gltf_image = input.images[i];
		textures[i].load_from_img(path + "/" + gltf_image.uri);
	}

	//texture_idxs.resize(input.textures.size());
	//for (auto i = 0; i < input.textures.size(); i++) {
	//	texture_idxs[i] = input.textures[i].source;
	//}
}

void GLTFModel::load_node(const tinygltf::Node& input_node, const tinygltf::Model& GLTFModel,
					  GLTFModel::Node* parent, std::vector<uint32_t>& idx_buffer,
					  std::vector<GLTFModel::Vertex>& vertex_buffer) {

	GLTFModel::Node node;
	node.name = input_node.name;
	// Load local TRS matrix
	node.matrix = glm::mat4(1.0f);
	if(input_node.translation.size() == 3) {
		node.matrix = glm::translate(node.matrix,
									 glm::vec3(glm::make_vec3(input_node.translation.data())));
	}
	if(input_node.rotation.size() == 4) {
		glm::quat q = glm::make_quat(input_node.rotation.data());
		node.matrix *= glm::mat4(q);
	}
	if(input_node.scale.size() == 3) {
		node.matrix = glm::scale(node.matrix,
								 glm::vec3(glm::make_vec3(input_node.scale.data())));
	}
	if(input_node.matrix.size() == 16) {
		node.matrix = glm::make_mat4x4(input_node.matrix.data());
	};

	// Load children
	if(input_node.children.size() > 0) {
		for(size_t i = 0; i < input_node.children.size(); i++) {
			load_node(GLTFModel.nodes[input_node.children[i]], GLTFModel, &node, idx_buffer, vertex_buffer);
		}
	}

	if(input_node.mesh > -1) {
		auto mesh = GLTFModel.meshes[input_node.mesh];

		for(const auto& primitive : mesh.primitives) {
			auto first_idx = static_cast<uint32_t>(idx_buffer.size());
			auto first_vertex = static_cast<uint32_t>(vertex_buffer.size());
			auto idx_count = 0;
			size_t vertex_count = 0;

			const float* positions = nullptr;
			const float* normals = nullptr;
			const float* tex_coords0 = nullptr;
			const float* tex_coords1 = nullptr;
			const float* tangents = nullptr;

			for(const auto& attrib : primitive.attributes) {
				const auto& accessor = GLTFModel.accessors[attrib.second];
				const auto& view = GLTFModel.bufferViews[accessor.bufferView];
				auto accessor_offset = accessor.byteOffset;
				auto buffer_view_offset = view.byteOffset;
				if(attrib.first == "POSITION") {
					positions = reinterpret_cast<const float*>(
						&(GLTFModel.buffers[view.buffer].data[accessor_offset + buffer_view_offset])
						);
					vertex_count = accessor.count;
				} else if(attrib.first == "NORMAL") {
					normals = reinterpret_cast<const float*>(
						&(GLTFModel.buffers[view.buffer].data[accessor_offset + buffer_view_offset])
						);
				} else if(attrib.first == "TEXCOORD_0") {
					tex_coords0 = reinterpret_cast<const float*>(
						&(GLTFModel.buffers[view.buffer].data[accessor_offset + buffer_view_offset])
						);
				} else if(attrib.first == "TEXCOORD_1") {
					tex_coords1 = reinterpret_cast<const float*>(
						&(GLTFModel.buffers[view.buffer].data[accessor_offset + buffer_view_offset])
						);
				} else if(attrib.first == "TANGENT") {
					tangents = reinterpret_cast<const float*>(
						&(GLTFModel.buffers[view.buffer].data[accessor_offset + buffer_view_offset])
						);
				}
			}

			vertex_buffer.reserve(vertex_buffer.size() + vertex_count);
			for(auto i = 0; i < vertex_count; i++) {
				Vertex vert{};
				vert.pos = glm::vec4(glm::make_vec3(&positions[i * 3]), 1.0f);
				vert.normal = glm::normalize(
					glm::vec3(normals ? glm::make_vec3(&normals[i * 3]) : glm::vec3(0.0f))
				);
				vert.uv0 = tex_coords0 ? glm::make_vec2(&tex_coords0[i * 2]) : glm::vec3(0.0f);
				vert.uv1 = tex_coords1 ? glm::make_vec2(&tex_coords1[i * 2]) : glm::vec3(0.0f);
				vert.color = glm::vec3(1.0f);
				vert.tangent = tangents ? glm::make_vec4(&tangents[i * 4]) : glm::vec4(0.0f);
				vertex_buffer.push_back(vert);

			}
			// Indices
			const auto& accessor = GLTFModel.accessors[primitive.indices];
			const auto& buffer_view = GLTFModel.bufferViews[accessor.bufferView];
			const auto& buffer = GLTFModel.buffers[buffer_view.buffer];
			idx_count += static_cast<uint32_t>(accessor.count);
			idx_buffer.reserve(idx_buffer.size() + accessor.count);
			switch(accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
				{
					uint32_t* buf = new uint32_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint32_t));
					for(size_t index = 0; index < accessor.count; index++) {
						idx_buffer.push_back(buf[index] + first_vertex);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
				{
					uint16_t* buf = new uint16_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint16_t));
					for(size_t index = 0; index < accessor.count; index++) {
						idx_buffer.push_back(buf[index] + first_vertex);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
				{
					uint8_t* buf = new uint8_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint8_t));
					for(size_t index = 0; index < accessor.count; index++) {
						idx_buffer.push_back(buf[index] + first_vertex);
					}
					break;
				}
				default:
					LUMEN_ERROR("Index component type is not supported");
					return;
			}

			Primitive pr;
			pr.first_idx = first_idx;
			pr.idx_cnt = idx_count;
			pr.material_idx = primitive.material;
			node.mesh.primitives.emplace_back(pr);
		}
	}

	if(parent) {
		parent->children.emplace_back(node);
	} else {
		nodes.emplace_back(node);
	}
}

void GLTFModel::draw_node(const VkCommandBuffer& cmd_buffer,
					  const VkPipelineLayout& pipeline_layout,
					  const GLTFModel::Node& node,
					  size_t cb_index, RenderFunction render_func) {
	if(!node.visible) {
		return;
	}
	if(node.mesh.primitives.size() > 0) {
		// To get the global transformation matrix, we need to traverse from the root
		auto global_matrix = node.matrix;
		Node* current_parent = node.parent;
		while(current_parent) {
			global_matrix = current_parent->matrix * global_matrix;
			current_parent = current_parent->parent;
		}
		vkCmdPushConstants(cmd_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &global_matrix);
		for(const auto& primitive : node.mesh.primitives) {
			if(primitive.idx_cnt) {
				auto& material = materials[primitive.material_idx];
				if(render_func) {
					render_func(&material, cmd_buffer, pipeline_layout, cb_index);
				} else {
					vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
					vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
											pipeline_layout, 0, 1, &material.descriptor_set, 0, nullptr
					);
				}
				vkCmdDrawIndexed(cmd_buffer, primitive.idx_cnt, 1, primitive.first_idx, 0, 0);
			}
		}
	}
	for(const auto& child : node.children) {
		draw_node(cmd_buffer, pipeline_layout, child, cb_index, render_func);
	}
}

void GLTFModel::draw(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout,
				 size_t cb_index, RenderFunction render_func) {
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.handle, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indices.handle, 0, VK_INDEX_TYPE_UINT32);
	for(const auto& node : nodes) {
		draw_node(commandBuffer, pipelineLayout, node, cb_index, render_func);
	}
}


void GLTFModel::destroy(VulkanContext* ctx) {
	vertices.destroy();
	indices.destroy();
	for(auto& texture : textures) {
		texture.destroy();
	}
	for(auto& material : materials) {
		if(material.pipeline) {
			vkDestroyPipeline(ctx->device, material.pipeline, nullptr);
		}
	}
}
