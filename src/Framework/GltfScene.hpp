/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

 /**
   These utilities are for loading glTF models in a
   canonical scene representation. From this representation
   you would create the appropriate 3D API resources (buffers
   and textures).

   \code{.cpp}
   // Typical Usage
   // Load the GLTF Scene using TinyGLTF

   tinygltf::Model    gltfModel;
   tinygltf::TinyGLTF gltfContext;
   fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warn,
   m_filename);

   // Fill the data in the gltfScene
   gltfScene.getMaterials(tmodel);
   gltfScene.getDrawableNodes(tmodel, GltfAttributes::Normal |
   GltfAttributes::Texcoord_0);

   // Todo in App:
   //   create buffers for vertices and indices, from gltfScene.m_position,
   gltfScene.m_index
   //   create textures from images: using tinygltf directly
   //   create descriptorSet for material using directly gltfScene.m_materials
   \endcode

 */

#pragma once
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/string_cast.hpp>
#include <tiny_gltf.h>
#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <robin-hood/robin_hood.h>
#include <vector>

#define KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME "KHR_lights_punctual"

 // https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness/README.md
#define KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME                     \
    "KHR_materials_pbrSpecularGlossiness"
struct KHR_materials_pbrSpecularGlossiness {
	glm::vec4 diffuseFactor{ 1.f, 1.f, 1.f, 1.f };
	int diffuseTexture{ -1 };
	glm::vec3 specularFactor{ 1.f, 1.f, 1.f };
	float glossinessFactor{ 1.f };
	int specularGlossinessTexture{ -1 };
};

// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_texture_transform
#define KHR_TEXTURE_TRANSFORM_EXTENSION_NAME "KHR_texture_transform"
struct KHR_texture_transform {
	glm::vec2 offset{ 0.f, 0.f };
	float rotation{ 0.f };
	glm::vec2 scale{ 1.f };
	int texCoord{ 0 };
	glm::mat3 uvTransform{ 1 }; // Computed transform of offset, rotation, scale
};

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_clearcoat/README.md
#define KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME "KHR_materials_clearcoat"
struct KHR_materials_clearcoat {
	float factor{ 0.f };
	int texture{ -1 };
	float roughnessFactor{ 0.f };
	int roughnessTexture{ -1 };
	int normalTexture{ -1 };
};

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_sheen/README.md
#define KHR_MATERIALS_SHEEN_EXTENSION_NAME "KHR_materials_sheen"
struct KHR_materials_sheen {
	glm::vec3 colorFactor{ 0.f, 0.f, 0.f };
	int colorTexture{ -1 };
	float roughnessFactor{ 0.f };
	int roughnessTexture{ -1 };
};

// https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_volume/extensions/2.0/Khronos/KHR_materials_transmission
#define KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME "KHR_materials_transmission"
struct KHR_materials_transmission {
	float factor{ 0.f };
	int texture{ -1 };
};

// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_unlit
#define KHR_MATERIALS_UNLIT_EXTENSION_NAME "KHR_materials_unlit"
struct KHR_materials_unlit {
	int active{ 0 };
};

// PBR Next : KHR_materials_anisotropy
#define KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME "KHR_materials_anisotropy"
struct KHR_materials_anisotropy {
	float factor{ 0.f };
	glm::vec3 direction{ 1.f, 0.f, 0.f };
	int texture{ -1 };
};

// https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_ior/extensions/2.0/Khronos/KHR_materials_ior
#define KHR_MATERIALS_IOR_EXTENSION_NAME "KHR_materials_ior"
struct KHR_materials_ior {
	float ior{ 1.5f };
};

// https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_volume/extensions/2.0/Khronos/KHR_materials_volume
#define KHR_MATERIALS_VOLUME_EXTENSION_NAME "KHR_materials_volume"
struct KHR_materials_volume {
	float thickness_factor{ 0 };
	int thickness_texture{ -1 };
	float attenuation_distance{ std::numeric_limits<float>::max() };
	glm::vec3 attenuation_color{ 1.f, 1.f, 1.f };
};

// https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-material
struct GltfMaterial {
	int shadingModel{ 0 }; // 0: metallic-roughness, 1: specular-glossiness

	// pbrMetallicRoughness
	glm::vec4 base_color_factor{ 1.f, 1.f, 1.f, 1.f };
	int base_color_texture{ -1 };
	float metallic_factor{ 1.f };
	float roughness_factor{ 1.f };
	int metallic_rougness_texture{ -1 };

	int emissive_texture{ -1 };
	glm::vec3 emissive_factor{ 0, 0, 0 };
	int alpha_mode{ 0 };
	float alpha_cutoff{ 0.5f };
	int double_sided{ 0 };

	int normal_texture{ -1 };
	float normal_texture_scale{ 1.f };
	int occlusion_texture{ -1 };
	float occlusion_texture_strength{ 1 };

	// Extensions
	KHR_materials_pbrSpecularGlossiness specular_glossiness;
	KHR_texture_transform texture_transform;
	KHR_materials_clearcoat clearcoat;
	KHR_materials_sheen sheen;
	KHR_materials_transmission transmission;
	KHR_materials_unlit unlit;
	KHR_materials_anisotropy anisotropy;
	KHR_materials_ior ior;
	KHR_materials_volume volume;
};

struct GltfNode {
	glm::mat4 world_matrix{ 1 };
	int prim_mesh{ 0 };
};

struct GltfPrimMesh {
	uint32_t first_idx{ 0 };
	uint32_t idx_count{ 0 };
	uint32_t vtx_offset{ 0 };
	uint32_t vtx_count{ 0 };
	int material_idx{ 0 };

	glm::vec3 pos_min{ 0, 0, 0 };
	glm::vec3 pos_max{ 0, 0, 0 };
	std::string name;
};

struct GltfStats {
	uint32_t nb_cameras{ 0 };
	uint32_t nb_images{ 0 };
	uint32_t nb_textures{ 0 };
	uint32_t nb_materials{ 0 };
	uint32_t nb_samplers{ 0 };
	uint32_t nb_nodes{ 0 };
	uint32_t nb_meshes{ 0 };
	uint32_t nb_lights{ 0 };
	uint32_t image_mem{ 0 };
	uint32_t nb_unique_triangles{ 0 };
	uint32_t nb_triangles{ 0 };
};

struct GltfCamera {
	glm::mat4 world_matrix{ 1 };
	glm::vec3 eye{ 0, 0, 0 };
	glm::vec3 center{ 0, 0, 0 };
	glm::vec3 up{ 0, 1, 0 };

	tinygltf::Camera cam;
};

// See:
// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md
struct GltfLight {
	glm::mat4 world_matrix{ 1 };
	tinygltf::Light light;
};

enum class GltfAttributes : uint8_t {
	Position = 0,
	Normal = 1,
	Texcoord_0 = 2,
	Texcoord_1 = 4,
	Tangent = 8,
	Color_0 = 16,
	// Joints_0   = 32, // #TODO - Add support for skinning
	// Weights_0  = 64,
};
using GltfAttributes_t = std::underlying_type_t<GltfAttributes>;

inline GltfAttributes operator|(GltfAttributes lhs, GltfAttributes rhs) {
	return static_cast<GltfAttributes>(static_cast<GltfAttributes_t>(lhs) |
									   static_cast<GltfAttributes_t>(rhs));
}

inline GltfAttributes operator&(GltfAttributes lhs, GltfAttributes rhs) {
	return static_cast<GltfAttributes>(static_cast<GltfAttributes_t>(lhs) &
									   static_cast<GltfAttributes_t>(rhs));
}

//--------------------------------------------------------------------------------------------------
// Class to convert gltfScene in simple draw-able format
//
struct GltfScene {
	void import_materials(const tinygltf::Model& tmodel);
	void import_drawable_nodes(const tinygltf::Model& tmodel,
							   GltfAttributes attributes);
	void compute_scene_dimensions();
	void destroy();

	static GltfStats get_statistics(const tinygltf::Model& tiny_model);

	// Scene data
	std::vector<GltfMaterial> materials;   // Material for shading
	std::vector<GltfNode> nodes;           // Drawable nodes, flat hierarchy
	std::vector<GltfPrimMesh> prim_meshes; // Primitive promoted to meshes
	std::vector<GltfCamera> cameras;
	std::vector<GltfLight> lights;

	// Attributes, all same length if valid
	std::vector<glm::vec3> positions;
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec4> tangents;
	std::vector<glm::vec2> texcoords0;
	std::vector<glm::vec2> texcoords1;
	std::vector<glm::vec4> colors0;

	// #TODO - Adding support for Skinning
	// using vec4us = vector4<unsigned short>;
	// std::vector<vec4us>        m_joints0;
	// std::vector<glm::vec4> m_weights0;

	// Size of the scene
	struct Dimensions {
		glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 max = glm::vec3(std::numeric_limits<float>::min());
		glm::vec3 size{ 0.f };
		glm::vec3 center{ 0.f };
		float radius{ 0 };
	} m_dimensions;

private:
	void process_node(const tinygltf::Model& tmodel, int& nodeIdx,
					  const glm::mat4& parentMatrix);
	void process_mesh(const tinygltf::Model& tmodel,
					  const tinygltf::Primitive& tmesh,
					  GltfAttributes attributes, const std::string& name);

	// Temporary data
	std::unordered_map<int, std::vector<uint32_t>> mesh_to_prim_meshes;
	std::vector<uint32_t> primitive_indices_32u;
	std::vector<uint16_t> primitive_indices_16u;
	std::vector<uint8_t> primitive_indices_8u;

	std::unordered_map<std::string, GltfPrimMesh> cache_prim_mesh;

	void compute_camera();
	void check_required_extensions(const tinygltf::Model& tmodel);
};

glm::mat4 get_local_matrix(const tinygltf::Node& tnode);

// Return a vector of data for a tinygltf::Value
template <typename T>
static inline std::vector<T> get_vector(const tinygltf::Value& value) {
	std::vector<T> result{ 0 };
	if (!value.IsArray())
		return result;
	result.resize(value.ArrayLen());
	for (int i = 0; i < value.ArrayLen(); i++) {
		result[i] =
			static_cast<T>(value.Get(i).IsNumber() ? value.Get(i).Get<double>()
						   : value.Get(i).Get<int>());
	}
	return result;
}

static inline void get_float(const tinygltf::Value& value,
							 const std::string& name, float& val) {
	if (value.Has(name)) {
		val = static_cast<float>(value.Get(name).Get<double>());
	}
}

static inline void get_int(const tinygltf::Value& value,
						   const std::string& name, int& val) {
	if (value.Has(name)) {
		val = value.Get(name).Get<int>();
	}
}

static inline void get_vec2(const tinygltf::Value& value,
							const std::string& name, glm::vec2& val) {
	if (value.Has(name)) {
		auto s = get_vector<float>(value.Get(name));
		val = glm::vec2{ s[0], s[1] };
	}
}

static inline void get_vec3(const tinygltf::Value& value,
							const std::string& name, glm::vec3& val) {
	if (value.Has(name)) {
		auto s = get_vector<float>(value.Get(name));
		val = glm::vec3{ s[0], s[1], s[2] };
	}
}

static inline void get_vec4(const tinygltf::Value& value,
							const std::string& name, glm::vec4& val) {
	if (value.Has(name)) {
		auto s = get_vector<float>(value.Get(name));
		val = glm::vec4{ s[0], s[1], s[2], s[3] };
	}
}

static inline void get_tex_id(const tinygltf::Value& value,
							  const std::string& name, int& val) {
	if (value.Has(name)) {
		val = value.Get(name).Get("index").Get<int>();
	}
}

// Appending to \p attribVec, all the values of \p attribName
// Return false if the attribute is missing
template <typename T>
static bool get_attribute(const tinygltf::Model& tmodel,
						  const tinygltf::Primitive& primitive,
						  std::vector<T>& attrib_vec,
						  const std::string& attrib_name) {
	if (primitive.attributes.find(attrib_name) == primitive.attributes.end())
		return false;

	// Retrieving the data of the attribute
	const auto& accessor =
		tmodel.accessors[primitive.attributes.find(attrib_name)->second];
	const auto& buf_view = tmodel.bufferViews[accessor.bufferView];
	const auto& buffer = tmodel.buffers[buf_view.buffer];
	const auto buf_data = reinterpret_cast<const T*>(
		&(buffer.data[accessor.byteOffset + buf_view.byteOffset]));
	const auto nb_elems = accessor.count;

	// Supporting KHR_mesh_quantization
	assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

	// Copying the attributes
	if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
		if (buf_view.byteStride == 0) {
			attrib_vec.insert(attrib_vec.end(), buf_data, buf_data + nb_elems);
		} else {
			// With stride, need to add one by one the element
			auto buffer_byte = reinterpret_cast<const uint8_t*>(buf_data);
			for (size_t i = 0; i < nb_elems; i++) {
				attrib_vec.push_back(*reinterpret_cast<const T*>(buffer_byte));
				buffer_byte += buf_view.byteStride;
			}
		}
	} else {
		// The component is smaller than float and need to be converted

		// VEC3 or VEC4
		int nb_components = accessor.type == TINYGLTF_TYPE_VEC2 ? 2
			: (accessor.type == TINYGLTF_TYPE_VEC3) ? 3
			: 4;
		// UNSIGNED_BYTE or UNSIGNED_SHORT
		size_t stride_component =
			accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ? 1
			: 2;

		size_t byte_stride = buf_view.byteStride > 0
			? buf_view.byteStride
			: size_t(nb_components) * stride_component;
		auto buffer_byte = reinterpret_cast<const uint8_t*>(buf_data);
		for (size_t i = 0; i < nb_elems; i++) {
			T vec_value;

			auto buffer_byte_data = buffer_byte;
			for (int c = 0; c < nb_components; c++) {
				float value =
					*reinterpret_cast<const float*>(buffer_byte_data);
				switch (accessor.componentType) {
					case TINYGLTF_COMPONENT_TYPE_BYTE:
						vec_value[c] = std::max(value / 127.f, -1.f);
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						vec_value[c] = value / 255.f;
						break;
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						vec_value[c] = std::max(value / 32767.f, -1.f);
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						vec_value[c] = value / 65535.f;
						break;
					default:
						assert(!"KHR_mesh_quantization unsupported format");
						break;
				}
				buffer_byte_data += stride_component;
			}
			buffer_byte += byte_stride;
			attrib_vec.push_back(vec_value);
		}
	}
	return true;
}

inline bool has_extension(const tinygltf::ExtensionMap& extensions,
						  const std::string& name) {
	return extensions.find(name) != extensions.end();
}
