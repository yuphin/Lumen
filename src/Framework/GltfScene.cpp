#include "GltfScene.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include "Logger.h"

namespace lumen {
#define EXTENSION_ATTRIB_IRAY "NV_attributes_iray"

struct Bbox {
	Bbox() = default;
	Bbox(glm::vec3 _min, glm::vec3 _max) : m_min(_min), m_max(_max) {}
	Bbox(const std::vector<glm::vec3>& corners) {
		for (auto& c : corners) {
			insert(c);
		}
	}

	void insert(const glm::vec3& v) {
		m_min = {std::min(m_min.x, v.x), std::min(m_min.y, v.y), std::min(m_min.z, v.z)};
		m_max = {std::max(m_max.x, v.x), std::max(m_max.y, v.y), std::max(m_max.z, v.z)};
	}

	void insert(const Bbox& b) {
		insert(b.m_min);
		insert(b.m_max);
	}

	inline Bbox& operator+=(float v) {
		m_min -= v;
		m_max += v;
		return *this;
	}

	inline bool is_empty() const {
		return m_min == glm::vec3{std::numeric_limits<float>::max()} ||
			   m_max == glm::vec3{std::numeric_limits<float>::lowest()};
	}
	inline uint32_t rank() const {
		uint32_t result{0};
		result += m_min.x < m_max.x;
		result += m_min.y < m_max.y;
		result += m_min.z < m_max.z;
		return result;
	}
	inline bool isPoint() const { return m_min == m_max; }
	inline bool isLine() const { return rank() == 1u; }
	inline bool isPlane() const { return rank() == 2u; }
	inline bool isVolume() const { return rank() == 3u; }
	inline glm::vec3 min() { return m_min; }
	inline glm::vec3 max() { return m_max; }
	inline glm::vec3 extents() { return m_max - m_min; }
	inline glm::vec3 center() { return (m_min + m_max) * 0.5f; }
	inline float radius() { return glm::length(m_max - m_min) * 0.5f; }

	Bbox transform(glm::mat4 mat) {
		std::vector<glm::vec3> corners(8);
		corners[0] = mat * glm::vec4(m_min.x, m_min.y, m_min.z, 1);
		corners[1] = mat * glm::vec4(m_min.x, m_min.y, m_max.z, 1);
		corners[2] = mat * glm::vec4(m_min.x, m_max.y, m_min.z, 1);
		corners[3] = mat * glm::vec4(m_min.x, m_max.y, m_max.z, 1);
		corners[4] = mat * glm::vec4(m_max.x, m_min.y, m_min.z, 1);
		corners[5] = mat * glm::vec4(m_max.x, m_min.y, m_max.z, 1);
		corners[6] = mat * glm::vec4(m_max.x, m_max.y, m_min.z, 1);
		corners[7] = mat * glm::vec4(m_max.x, m_max.y, m_max.z, 1);

		Bbox result(corners);
		return result;
	}

   private:
	glm::vec3 m_min{std::numeric_limits<float>::max()};
	glm::vec3 m_max{std::numeric_limits<float>::lowest()};
};

//--------------------------------------------------------------------------------------------------
// Collect the value of all materials
//
void GltfScene::import_materials(const tinygltf::Model& tmodel) {
	materials.reserve(tmodel.materials.size());

	for (auto& tmat : tmodel.materials) {
		GltfMaterial gmat;

		gmat.alpha_cutoff = static_cast<float>(tmat.alphaCutoff);
		gmat.alpha_mode = tmat.alphaMode == "MASK" ? 1 : (tmat.alphaMode == "BLEND" ? 2 : 0);
		gmat.double_sided = tmat.doubleSided ? 1 : 0;
		gmat.emissive_factor = glm::vec3(tmat.emissiveFactor[0], tmat.emissiveFactor[1], tmat.emissiveFactor[2]);
		gmat.emissive_texture = tmat.emissiveTexture.index;
		gmat.normal_texture = tmat.normalTexture.index;
		gmat.normal_texture_scale = static_cast<float>(tmat.normalTexture.scale);
		gmat.occlusion_texture = tmat.occlusionTexture.index;
		gmat.occlusion_texture_strength = static_cast<float>(tmat.occlusionTexture.strength);

		// PbrMetallicRoughness
		auto& tpbr = tmat.pbrMetallicRoughness;
		gmat.base_color_factor = glm::vec4(tpbr.baseColorFactor[0], tpbr.baseColorFactor[1], tpbr.baseColorFactor[2],
										   tpbr.baseColorFactor[3]);
		gmat.base_color_texture = tpbr.baseColorTexture.index;
		gmat.metallic_factor = static_cast<float>(tpbr.metallicFactor);
		gmat.metallic_rougness_texture = tpbr.metallicRoughnessTexture.index;
		gmat.roughness_factor = static_cast<float>(tpbr.roughnessFactor);

		// KHR_materials_pbrSpecularGlossiness
		if (tmat.extensions.find(KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME) != tmat.extensions.end()) {
			gmat.shadingModel = 1;

			const auto& ext = tmat.extensions.find(KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME)->second;
			get_vec4(ext, "diffuseFactor", gmat.specular_glossiness.diffuseFactor);
			get_float(ext, "glossinessFactor", gmat.specular_glossiness.glossinessFactor);
			get_vec3(ext, "specularFactor", gmat.specular_glossiness.specularFactor);
			get_tex_id(ext, "diffuseTexture", gmat.specular_glossiness.diffuseTexture);
			get_tex_id(ext, "specularGlossinessTexture", gmat.specular_glossiness.specularGlossinessTexture);
		}

		// KHR_texture_transform
		if (tpbr.baseColorTexture.extensions.find(KHR_TEXTURE_TRANSFORM_EXTENSION_NAME) !=
			tpbr.baseColorTexture.extensions.end()) {
			const auto& ext = tpbr.baseColorTexture.extensions.find(KHR_TEXTURE_TRANSFORM_EXTENSION_NAME)->second;
			auto& tt = gmat.texture_transform;
			get_vec2(ext, "offset", tt.offset);
			get_vec2(ext, "scale", tt.scale);
			get_float(ext, "rotation", tt.rotation);
			get_int(ext, "texCoord", tt.texCoord);

			// Computing the transformation
			auto translation = glm::mat3(1, 0, tt.offset.x, 0, 1, tt.offset.y, 0, 0, 1);
			auto rotation =
				glm::mat3(cos(tt.rotation), sin(tt.rotation), 0, -sin(tt.rotation), cos(tt.rotation), 0, 0, 0, 1);
			auto scale = glm::mat3(tt.scale.x, 0, 0, 0, tt.scale.y, 0, 0, 0, 1);
			tt.uvTransform = scale * rotation * translation;
		}

		// KHR_materials_unlit
		if (tmat.extensions.find(KHR_MATERIALS_UNLIT_EXTENSION_NAME) != tmat.extensions.end()) {
			gmat.unlit.active = 1;
		}

		// KHR_materials_anisotropy
		if (tmat.extensions.find(KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME) != tmat.extensions.end()) {
			const auto& ext = tmat.extensions.find(KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME)->second;
			get_float(ext, "anisotropy", gmat.anisotropy.factor);
			get_vec3(ext, "anisotropyDirection", gmat.anisotropy.direction);
			get_tex_id(ext, "anisotropyTexture", gmat.anisotropy.texture);
		}

		// KHR_materials_clearcoat
		if (tmat.extensions.find(KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME) != tmat.extensions.end()) {
			const auto& ext = tmat.extensions.find(KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME)->second;
			get_float(ext, "clearcoatFactor", gmat.clearcoat.factor);
			get_tex_id(ext, "clearcoatTexture", gmat.clearcoat.texture);
			get_float(ext, "clearcoatRoughnessFactor", gmat.clearcoat.roughnessFactor);
			get_tex_id(ext, "clearcoatRoughnessTexture", gmat.clearcoat.roughnessTexture);
			get_tex_id(ext, "clearcoatNormalTexture", gmat.clearcoat.normalTexture);
		}

		// KHR_materials_sheen
		if (tmat.extensions.find(KHR_MATERIALS_SHEEN_EXTENSION_NAME) != tmat.extensions.end()) {
			const auto& ext = tmat.extensions.find(KHR_MATERIALS_SHEEN_EXTENSION_NAME)->second;
			get_vec3(ext, "sheenColorFactor", gmat.sheen.colorFactor);
			get_tex_id(ext, "sheenColorTexture", gmat.sheen.colorTexture);
			get_float(ext, "sheenRoughnessFactor", gmat.sheen.roughnessFactor);
			get_tex_id(ext, "sheenRoughnessTexture", gmat.sheen.roughnessTexture);
		}

		// KHR_materials_transmission
		if (tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME) != tmat.extensions.end()) {
			const auto& ext = tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME)->second;
			get_float(ext, "transmissionFactor", gmat.transmission.factor);
			get_tex_id(ext, "transmissionTexture", gmat.transmission.texture);
		}

		// KHR_materials_ior
		if (tmat.extensions.find(KHR_MATERIALS_IOR_EXTENSION_NAME) != tmat.extensions.end()) {
			const auto& ext = tmat.extensions.find(KHR_MATERIALS_IOR_EXTENSION_NAME)->second;
			get_float(ext, "ior", gmat.ior.ior);
		}

		// KHR_materials_volume
		if (tmat.extensions.find(KHR_MATERIALS_VOLUME_EXTENSION_NAME) != tmat.extensions.end()) {
			const auto& ext = tmat.extensions.find(KHR_MATERIALS_VOLUME_EXTENSION_NAME)->second;
			get_float(ext, "thicknessFactor", gmat.volume.thickness_factor);
			get_tex_id(ext, "thicknessTexture", gmat.volume.thickness_texture);
			get_float(ext, "attenuationDistance", gmat.volume.attenuation_distance);
			get_vec3(ext, "attenuationColor", gmat.volume.attenuation_color);
		};

		materials.emplace_back(gmat);
	}

	// Make default
	if (materials.empty()) {
		GltfMaterial gmat;
		gmat.metallic_factor = 0;
		materials.emplace_back(gmat);
	}
}

//--------------------------------------------------------------------------------------------------
// Linearize the scene graph to world space nodes.
//
void GltfScene::import_drawable_nodes(const tinygltf::Model& tmodel, GltfAttributes attributes) {
	check_required_extensions(tmodel);

	// Find the number of vertex(attributes) and index
	// uint32_t nbVert{0};
	uint32_t nbIndex{0};
	uint32_t meshCnt{0};  // use for mesh to new meshes
	uint32_t primCnt{0};  //  "   "  "  "
	for (const auto& mesh : tmodel.meshes) {
		std::vector<uint32_t> vprim;
		for (const auto& primitive : mesh.primitives) {
			if (primitive.mode != 4)  // Triangle
				continue;
			const auto& posAccessor = tmodel.accessors[primitive.attributes.find("POSITION")->second];
			// nbVert += static_cast<uint32_t>(posAccessor.count);
			if (primitive.indices > -1) {
				const auto& indexAccessor = tmodel.accessors[primitive.indices];
				nbIndex += static_cast<uint32_t>(indexAccessor.count);
			} else {
				nbIndex += static_cast<uint32_t>(posAccessor.count);
			}
			vprim.emplace_back(primCnt++);
		}
		mesh_to_prim_meshes[meshCnt++] = std::move(vprim);	// mesh-id = { prim0, prim1, ... }
	}

	// Reserving memory
	indices.reserve(nbIndex);

	// Convert all mesh/primitives+ to a single primitive per mesh
	for (const auto& tmesh : tmodel.meshes) {
		for (const auto& tprimitive : tmesh.primitives) {
			process_mesh(tmodel, tprimitive, attributes, tmesh.name);
		}
	}

	// Transforming the scene hierarchy to a flat list
	int defaultScene = tmodel.defaultScene > -1 ? tmodel.defaultScene : 0;
	const auto& tscene = tmodel.scenes[defaultScene];
	for (auto nodeIdx : tscene.nodes) {
		process_node(tmodel, nodeIdx, glm::mat4(1));
	}

	compute_scene_dimensions();
	compute_camera();

	mesh_to_prim_meshes.clear();
	primitive_indices_32u.clear();
	primitive_indices_16u.clear();
	primitive_indices_8u.clear();
}

//--------------------------------------------------------------------------------------------------
//
//
void GltfScene::process_node(const tinygltf::Model& tmodel, int& nodeIdx, const glm::mat4& parentMatrix) {
	const auto& tnode = tmodel.nodes[nodeIdx];

	glm::mat4 matrix = get_local_matrix(tnode);
	glm::mat4 worldMatrix = parentMatrix * matrix;

	if (tnode.mesh > -1) {
		const auto& meshes = mesh_to_prim_meshes[tnode.mesh];  // A mesh could have many
															   // primitives
		for (const auto& mesh : meshes) {
			GltfNode node;
			node.prim_mesh = mesh;
			node.world_matrix = worldMatrix;
			nodes.emplace_back(node);
		}
	} else if (tnode.camera > -1) {
		GltfCamera camera;
		camera.world_matrix = worldMatrix;
		camera.cam = tmodel.cameras[tmodel.nodes[nodeIdx].camera];

		// If the node has the Iray extension, extract the camera information.
		if (has_extension(tnode.extensions, EXTENSION_ATTRIB_IRAY)) {
			auto& iray_ext = tnode.extensions.at(EXTENSION_ATTRIB_IRAY);
			auto& attributes = iray_ext.Get("attributes");
			for (size_t idx = 0; idx < attributes.ArrayLen(); idx++) {
				auto& attrib = attributes.Get((int)idx);
				std::string att_name = attrib.Get("name").Get<std::string>();
				auto& att_value = attrib.Get("value");
				if (att_value.IsArray()) {
					auto vec = get_vector<float>(att_value);
					if (att_name == "iview:position")
						camera.eye = {vec[0], vec[1], vec[2]};
					else if (att_name == "iview:interest")
						camera.center = {vec[0], vec[1], vec[2]};
					else if (att_name == "iview:up")
						camera.up = {vec[0], vec[1], vec[2]};
				}
			}
		}

		cameras.emplace_back(camera);
	} else if (tnode.extensions.find(KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME) != tnode.extensions.end()) {
		GltfLight light;
		const auto& ext = tnode.extensions.find(KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME)->second;
		auto light_idx = ext.Get("light").GetNumberAsInt();
		light.light = tmodel.lights[(int)light_idx];
		light.world_matrix = worldMatrix;
		lights.emplace_back(light);
	}

	// Recursion for all children
	for (auto child : tnode.children) {
		process_node(tmodel, child, worldMatrix);
	}
}

//--------------------------------------------------------------------------------------------------
// Extracting the values to a linear buffer
//
void GltfScene::process_mesh(const tinygltf::Model& tmodel, const tinygltf::Primitive& tmesh, GltfAttributes attributes,
							 const std::string& name) {
	// Only triangles are supported
	// 0:point, 1:lines, 2:line_loop, 3:line_strip, 4:triangles,
	// 5:triangle_strip, 6:triangle_fan
	if (tmesh.mode != 4) return;

	GltfPrimMesh result_mesh;
	result_mesh.name = name;
	result_mesh.material_idx = std::max(0, tmesh.material);
	result_mesh.vtx_offset = static_cast<uint32_t>(positions.size());
	result_mesh.first_idx = static_cast<uint32_t>(indices.size());

	// Create a key made of the attributes, to see if the primitive was already
	// processed. If it is, we will re-use the cache, but allow the material and
	// indices to be different.
	std::stringstream o;
	for (auto& a : tmesh.attributes) {
		o << a.first << a.second;
	}
	std::string key = o.str();
	bool prim_mesh_cached = false;

	// Found a cache - will not need to append vertex
	auto it = cache_prim_mesh.find(key);
	if (it != cache_prim_mesh.end()) {
		prim_mesh_cached = true;
		GltfPrimMesh cacheMesh = it->second;
		result_mesh.vtx_count = cacheMesh.vtx_count;
		result_mesh.vtx_offset = cacheMesh.vtx_offset;
	}

	// INDICES
	if (tmesh.indices > -1) {
		const tinygltf::Accessor& index_accessor = tmodel.accessors[tmesh.indices];
		const tinygltf::BufferView& buffer_view = tmodel.bufferViews[index_accessor.bufferView];
		const tinygltf::Buffer& buffer = tmodel.buffers[buffer_view.buffer];

		result_mesh.idx_count = static_cast<uint32_t>(index_accessor.count);

		switch (index_accessor.componentType) {
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
				primitive_indices_32u.resize(index_accessor.count);
				memcpy(primitive_indices_32u.data(), &buffer.data[index_accessor.byteOffset + buffer_view.byteOffset],
					   index_accessor.count * sizeof(uint32_t));
				indices.insert(indices.end(), primitive_indices_32u.begin(), primitive_indices_32u.end());
				break;
			}
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
				primitive_indices_16u.resize(index_accessor.count);
				memcpy(primitive_indices_16u.data(), &buffer.data[index_accessor.byteOffset + buffer_view.byteOffset],
					   index_accessor.count * sizeof(uint16_t));
				indices.insert(indices.end(), primitive_indices_16u.begin(), primitive_indices_16u.end());
				break;
			}
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
				primitive_indices_8u.resize(index_accessor.count);
				memcpy(primitive_indices_8u.data(), &buffer.data[index_accessor.byteOffset + buffer_view.byteOffset],
					   index_accessor.count * sizeof(uint8_t));
				indices.insert(indices.end(), primitive_indices_8u.begin(), primitive_indices_8u.end());
				break;
			}
			default:
				std::cerr << "Index component type " << index_accessor.componentType << " not supported!" << std::endl;
				return;
		}
	} else {
		// Primitive without indices, creating them
		const auto& accessor = tmodel.accessors[tmesh.attributes.find("POSITION")->second];
		for (auto i = 0; i < accessor.count; i++) indices.push_back(i);
		result_mesh.idx_count = static_cast<uint32_t>(accessor.count);
	}

	if (prim_mesh_cached == false)	// Need to add this primitive
	{
		// POSITION
		{
			bool result = get_attribute<glm::vec3>(tmodel, tmesh, positions, "POSITION");

			// Keeping the size of this primitive (Spec says this is required
			// information)
			const auto& accessor = tmodel.accessors[tmesh.attributes.find("POSITION")->second];
			result_mesh.vtx_count = static_cast<uint32_t>(accessor.count);
			if (!accessor.minValues.empty())
				result_mesh.pos_min = glm::vec3(accessor.minValues[0], accessor.minValues[1], accessor.minValues[2]);
			if (!accessor.maxValues.empty())
				result_mesh.pos_max = glm::vec3(accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2]);
		}

		// NORMAL
		if ((attributes & GltfAttributes::Normal) == GltfAttributes::Normal) {
			if (!get_attribute<glm::vec3>(tmodel, tmesh, normals, "NORMAL")) {
				// Need to compute the normals
				std::vector<glm::vec3> geonormal(result_mesh.vtx_count);
				for (size_t i = 0; i < result_mesh.idx_count; i += 3) {
					uint32_t ind0 = indices[result_mesh.first_idx + i + 0];
					uint32_t ind1 = indices[result_mesh.first_idx + i + 1];
					uint32_t ind2 = indices[result_mesh.first_idx + i + 2];
					const auto& pos0 = positions[ind0 + result_mesh.vtx_offset];
					const auto& pos1 = positions[ind1 + result_mesh.vtx_offset];
					const auto& pos2 = positions[ind2 + result_mesh.vtx_offset];
					const auto v1 = glm::normalize(pos1 - pos0);  // Many normalize, but when objects are
																  // really small the
					const auto v2 = glm::normalize(pos2 - pos0);  // cross will go below nv_eps and the
																  // normal will be (0,0,0)
					const auto n = glm::cross(v2, v1);
					geonormal[ind0] += n;
					geonormal[ind1] += n;
					geonormal[ind2] += n;
				}
				for (auto& n : geonormal) n = glm::normalize(n);
				normals.insert(normals.end(), geonormal.begin(), geonormal.end());
			}
		}

		// TEXCOORD_0
		if ((attributes & GltfAttributes::Texcoord_0) == GltfAttributes::Texcoord_0) {
			if (!get_attribute<glm::vec2>(tmodel, tmesh, texcoords0, "TEXCOORD_0")) {
				// Set them all to zero
				//      m_texcoords0.insert(m_texcoords0.end(),
				//      resultMesh.vertexCount, nvmath::vec2f(0, 0));

				// Cube map projection
				for (uint32_t i = 0; i < result_mesh.vtx_count; i++) {
					const auto& pos = positions[result_mesh.vtx_offset + i];
					float absx = fabs(pos.x);
					float absy = fabs(pos.y);
					float absz = fabs(pos.z);

					int is_x_positive = pos.x > 0 ? 1 : 0;
					int is_y_positive = pos.y > 0 ? 1 : 0;
					int is_z_positive = pos.z > 0 ? 1 : 0;

					float maxAxis, uc, vc;

					// POSITIVE X
					if (is_x_positive && absx >= absy && absx >= absz) {
						// u (0 to 1) goes from +z to -z
						// v (0 to 1) goes from -y to +y
						maxAxis = absx;
						uc = -pos.z;
						vc = pos.y;
					}
					// NEGATIVE X
					if (!is_x_positive && absx >= absy && absx >= absz) {
						// u (0 to 1) goes from -z to +z
						// v (0 to 1) goes from -y to +y
						maxAxis = absx;
						uc = pos.z;
						vc = pos.y;
					}
					// POSITIVE Y
					if (is_y_positive && absy >= absx && absy >= absz) {
						// u (0 to 1) goes from -x to +x
						// v (0 to 1) goes from +z to -z
						maxAxis = absy;
						uc = pos.x;
						vc = -pos.z;
					}
					// NEGATIVE Y
					if (!is_y_positive && absy >= absx && absy >= absz) {
						// u (0 to 1) goes from -x to +x
						// v (0 to 1) goes from -z to +z
						maxAxis = absy;
						uc = pos.x;
						vc = pos.z;
					}
					// POSITIVE Z
					if (is_z_positive && absz >= absx && absz >= absy) {
						// u (0 to 1) goes from -x to +x
						// v (0 to 1) goes from -y to +y
						maxAxis = absz;
						uc = pos.x;
						vc = pos.y;
					}
					// NEGATIVE Z
					if (!is_z_positive && absz >= absx && absz >= absy) {
						// u (0 to 1) goes from +x to -x
						// v (0 to 1) goes from -y to +y
						maxAxis = absz;
						uc = -pos.x;
						vc = pos.y;
					}

					// Convert range from -1 to 1 to 0 to 1
					float u = 0.5f * (uc / maxAxis + 1.0f);
					float v = 0.5f * (vc / maxAxis + 1.0f);

					texcoords0.emplace_back(u, v);
				}
			}
		}

		// TANGENT
		if ((attributes & GltfAttributes::Tangent) == GltfAttributes::Tangent) {
			if (!get_attribute<glm::vec4>(tmodel, tmesh, tangents, "TANGENT")) {
				// #TODO - Should calculate tangents using default MikkTSpace
				// algorithms See: https://github.com/mmikk/MikkTSpace

				std::vector<glm::vec3> tangent(result_mesh.vtx_count);
				std::vector<glm::vec3> bitangent(result_mesh.vtx_count);

				// Current implementation
				// http://foundationsofgameenginedev.com/FGED2-sample.pdf
				for (size_t i = 0; i < result_mesh.idx_count; i += 3) {
					// local index
					uint32_t i0 = indices[result_mesh.first_idx + i + 0];
					uint32_t i1 = indices[result_mesh.first_idx + i + 1];
					uint32_t i2 = indices[result_mesh.first_idx + i + 2];
					assert(i0 < result_mesh.vtx_count);
					assert(i1 < result_mesh.vtx_count);
					assert(i2 < result_mesh.vtx_count);

					// global index
					uint32_t gi0 = i0 + result_mesh.vtx_offset;
					uint32_t gi1 = i1 + result_mesh.vtx_offset;
					uint32_t gi2 = i2 + result_mesh.vtx_offset;

					const auto& p0 = positions[gi0];
					const auto& p1 = positions[gi1];
					const auto& p2 = positions[gi2];

					const auto& uv0 = texcoords0[gi0];
					const auto& uv1 = texcoords0[gi1];
					const auto& uv2 = texcoords0[gi2];

					glm::vec3 e1 = p1 - p0;
					glm::vec3 e2 = p2 - p0;

					glm::vec2 duvE1 = uv1 - uv0;
					glm::vec2 duvE2 = uv2 - uv0;

					float r = 1.0F;
					float a = duvE1.x * duvE2.y - duvE2.x * duvE1.y;
					if (fabs(a) > 0)  // Catch degenerated UV
					{
						r = 1.0f / a;
					}

					glm::vec3 t = (e1 * duvE2.y - e2 * duvE1.y) * r;
					glm::vec3 b = (e2 * duvE1.x - e1 * duvE2.x) * r;

					tangent[i0] += t;
					tangent[i1] += t;
					tangent[i2] += t;

					bitangent[i0] += b;
					bitangent[i1] += b;
					bitangent[i2] += b;
				}

				for (uint32_t a = 0; a < result_mesh.vtx_count; a++) {
					const auto& t = tangent[a];
					const auto& b = bitangent[a];
					const auto& n = normals[result_mesh.vtx_offset + a];

					// Gram-Schmidt orthogonalize
					glm::vec3 tangent = glm::normalize(t - (glm::dot(n, t) * n));

					// Calculate handedness
					float handedness = (glm::dot(glm::cross(n, t), b) < 0.0F) ? -1.0F : 1.0F;
					tangents.emplace_back(tangent.x, tangent.y, tangent.z, handedness);
				}
			}
		}

		// COLOR_0
		if ((attributes & GltfAttributes::Color_0) == GltfAttributes::Color_0) {
			if (!get_attribute<glm::vec4>(tmodel, tmesh, colors0, "COLOR_0")) {
				// Set them all to one
				colors0.insert(colors0.end(), result_mesh.vtx_count, glm::vec4(1, 1, 1, 1));
			}
		}
	}

	// Keep result in cache
	cache_prim_mesh[key] = result_mesh;

	// Append prim mesh to the list of all primitive meshes
	prim_meshes.emplace_back(result_mesh);
}

//--------------------------------------------------------------------------------------------------
// Return the matrix of the node
//
glm::mat4 get_local_matrix(const tinygltf::Node& tnode) {
	glm::mat4 mtranslation{1};
	glm::mat4 mscale{1};
	glm::mat4 mrot{1};
	glm::mat4 matrix{1};
	glm::quat mrotation;

	if (!tnode.translation.empty())
		mtranslation =
			glm::translate(mtranslation, glm::vec3(tnode.translation[0], tnode.translation[1], tnode.translation[2]));
	if (!tnode.scale.empty()) mscale = glm::scale(mscale, glm::vec3(tnode.scale[0], tnode.scale[1], tnode.scale[2]));
	if (!tnode.rotation.empty()) {
		mrotation[0] = static_cast<float>(tnode.rotation[0]);
		mrotation[1] = static_cast<float>(tnode.rotation[1]);
		mrotation[2] = static_cast<float>(tnode.rotation[2]);
		mrotation[3] = static_cast<float>(tnode.rotation[3]);
		mrot = glm::toMat4(mrotation);
	}
	if (!tnode.matrix.empty()) {
		float nodes[16];
		for (int i = 0; i < 16; ++i) nodes[i] = static_cast<float>(tnode.matrix[i]);
		matrix = glm::make_mat4(nodes);
	}
	return mtranslation * mrot * mscale * matrix;
}

void GltfScene::destroy() {
	materials.clear();
	nodes.clear();
	prim_meshes.clear();
	cameras.clear();
	lights.clear();

	positions.clear();
	indices.clear();
	normals.clear();
	tangents.clear();
	texcoords0.clear();
	texcoords1.clear();
	colors0.clear();
	cameras.clear();
	// m_joints0.clear();
	// m_weights0.clear();
	m_dimensions = {};
}

//--------------------------------------------------------------------------------------------------
// Get the dimension of the scene
//
void GltfScene::compute_scene_dimensions() {
	Bbox scnBbox;

	for (const auto& node : nodes) {
		const auto& mesh = prim_meshes[node.prim_mesh];

		Bbox bbox(mesh.pos_min, mesh.pos_max);
		bbox.transform(node.world_matrix);
		scnBbox.insert(bbox);
	}

	if (scnBbox.is_empty() || !scnBbox.isVolume()) {
		LUMEN_INFO("glTF: Scene bounding box invalid, Setting to: [-1,-1,-1], [1,1,1]");
		scnBbox.insert({-1.0f, -1.0f, -1.0f});
		scnBbox.insert({1.0f, 1.0f, 1.0f});
	}

	m_dimensions.min = scnBbox.min();
	m_dimensions.max = scnBbox.max();
	m_dimensions.size = scnBbox.extents();
	m_dimensions.center = scnBbox.center();
	m_dimensions.radius = scnBbox.radius();
}

static uint32_t recursive_triangle_count(const tinygltf::Model& model, int node_idx,
										 const std::vector<uint32_t>& mesh_triangle) {
	auto& node = model.nodes[node_idx];
	uint32_t nb_triangles{0};
	for (const auto child : node.children) {
		nb_triangles += recursive_triangle_count(model, child, mesh_triangle);
	}

	if (node.mesh >= 0) nb_triangles += mesh_triangle[node.mesh];

	return nb_triangles;
}

//--------------------------------------------------------------------------------------------------
// Retrieving information about the scene
//
GltfStats GltfScene::get_statistics(const tinygltf::Model& tinyModel) {
	GltfStats stats;

	stats.nb_cameras = static_cast<uint32_t>(tinyModel.cameras.size());
	stats.nb_images = static_cast<uint32_t>(tinyModel.images.size());
	stats.nb_textures = static_cast<uint32_t>(tinyModel.textures.size());
	stats.nb_materials = static_cast<uint32_t>(tinyModel.materials.size());
	stats.nb_samplers = static_cast<uint32_t>(tinyModel.samplers.size());
	stats.nb_nodes = static_cast<uint32_t>(tinyModel.nodes.size());
	stats.nb_meshes = static_cast<uint32_t>(tinyModel.meshes.size());
	stats.nb_lights = static_cast<uint32_t>(tinyModel.lights.size());

	// Computing the memory usage for images
	for (const auto& image : tinyModel.images) {
		stats.image_mem += image.width * image.height * image.component * image.bits / 8;
	}

	// Computing the number of triangles
	std::vector<uint32_t> mesh_triangle(tinyModel.meshes.size());
	uint32_t meshIdx{0};
	for (const auto& mesh : tinyModel.meshes) {
		for (const auto& primitive : mesh.primitives) {
			if (primitive.indices > -1) {
				const tinygltf::Accessor& indexAccessor = tinyModel.accessors[primitive.indices];
				mesh_triangle[meshIdx] += static_cast<uint32_t>(indexAccessor.count) / 3;
			} else {
				const auto& pos_accessor = tinyModel.accessors[primitive.attributes.find("POSITION")->second];
				mesh_triangle[meshIdx] += static_cast<uint32_t>(pos_accessor.count) / 3;
			}
		}
		meshIdx++;
	}

	stats.nb_unique_triangles = std::accumulate(mesh_triangle.begin(), mesh_triangle.end(), 0, std::plus<>());
	for (auto& node : tinyModel.scenes[0].nodes) {
		stats.nb_triangles += recursive_triangle_count(tinyModel, node, mesh_triangle);
	}

	return stats;
}

//--------------------------------------------------------------------------------------------------
// Going through all cameras and find the position and center of interest.
// - The eye or position of the camera is found in the translation part of the
// matrix
// - The center of interest is arbitrary set in front of the camera to a
// distance equivalent
//   to the eye and the center of the scene. If the camera is pointing toward
//   the middle of the scene, the camera center will be equal to the scene
//   center.
// - The up vector is always Y up for now.
//
void GltfScene::compute_camera() {
	for (auto& camera : cameras) {
		if (camera.eye == camera.center)  // Applying the rule only for uninitialized camera.
		{
			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(camera.world_matrix, scale, rotation, translation, skew, perspective);
			camera.eye = translation;
			float distance = glm::length(m_dimensions.center - camera.eye);
			camera.center = {0, 0, -distance};
			camera.center = camera.eye + (rotation * camera.center);
			camera.up = {0, 1, 0};
		}
	}
}

void GltfScene::check_required_extensions(const tinygltf::Model& tmodel) {
	std::set<std::string> supportedExtensions{
		KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME,
		KHR_TEXTURE_TRANSFORM_EXTENSION_NAME,
		KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME,
		KHR_MATERIALS_UNLIT_EXTENSION_NAME,
		KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME,
		KHR_MATERIALS_IOR_EXTENSION_NAME,
		KHR_MATERIALS_VOLUME_EXTENSION_NAME,
		KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME,
	};

	for (auto& e : tmodel.extensionsRequired) {
		if (supportedExtensions.find(e) == supportedExtensions.end()) {
			LUMEN_CRITICAL(
				"\n---------------------------------------\n"
				"The extension {} is REQUIRED and not supported",
				e.c_str());
		}
	}
}

}  // namespace lumen
