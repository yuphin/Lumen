#include <LumenPCH.h>
#include "Framework/VkUtils.h"
#include "Framework/BBox.h"
#include "LumenScene.h"
#pragma warning(push, 0)
#include <tinygltf/json.hpp>
#pragma warning(pop)
#include <tiny_obj_loader.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include "shaders/commons.h"
#include <cctype>
#include "Framework/PersistentResourceManager.h"

static bool ends_with(const std::string& str, const std::string& end) {
	if (end.size() > str.size()) return false;
	return std::equal(end.rbegin(), end.rend(), str.rbegin());
}

using json = nlohmann::json;
static float get_or_default_f(json& json, const std::string& prop, float val) {
	return json[prop].is_null() ? val : (float)json[prop];
}

static glm::vec3 get_or_default_v(json& json, const std::string& prop, const glm::vec3& val) {
	return json[prop].is_null() ? val : glm::vec3{json[prop][0], json[prop][1], json[prop][2]};
}

static uint32_t get_or_default_u(json& json, const std::string& prop, uint32_t val) {
	return json[prop].is_null() ? val : (uint32_t)json[prop];
}

static void reflectance_to_conductor_eta_k(const glm::vec3& reflectance, glm::vec3& eta, glm::vec3& k) {
	eta = glm::vec3(1.0f);
	k = 2.0f * glm::sqrt(reflectance) / glm::sqrt(glm::max(glm::vec3(1.0f) - reflectance, 0.001f));
};

void LumenScene::load_scene(const std::string& path) {
	if (ends_with(path, ".json")) {
		load_lumen_scene(path);
	} else if (ends_with(path, ".xml")) {
		load_mitsuba_scene(path);
	}

	const float aspect_ratio = (float)Window::width() / Window::height();
	if (config->cam_settings.pos != vec3(0)) {
		camera = std::unique_ptr<lumen::PerspectiveCamera>(
			new lumen::PerspectiveCamera(config->cam_settings.fov, 0.01f, 1000.0f, aspect_ratio,
										 config->cam_settings.dir, config->cam_settings.pos));
	} else {
		// Assume the camera matrix is given
		camera = std::unique_ptr<lumen::PerspectiveCamera>(new lumen::PerspectiveCamera(
			config->cam_settings.fov, config->cam_settings.cam_matrix, 0.01f, 1000.0f, aspect_ratio));
	}

	total_light_triangle_cnt = 0;
	total_light_area = 0;
	std::vector<PrimMeshInfo> prim_lookup;
	uint32_t idx = 0;
	for (auto& pm : prim_meshes) {
		PrimMeshInfo m_info;
		m_info.index_offset = pm.first_idx;
		m_info.vertex_offset = pm.vtx_offset;
		m_info.material_index = pm.material_idx;
		m_info.min_pos = glm::vec4(pm.min_pos, 0);
		m_info.max_pos = glm::vec4(pm.max_pos, 0);
		m_info.material_index = pm.material_idx;
		prim_lookup.emplace_back(m_info);
		auto& mef = materials[pm.material_idx].emissive_factor;
		if (mef.x > 0 || mef.y > 0 || mef.z > 0) {
			Light light;
			light.world_matrix = pm.world_matrix;
			light.num_triangles = pm.idx_count / 3;
			light.prim_mesh_idx = idx;
			light.light_flags = LIGHT_AREA;
			// Is finite
			light.light_flags |= 1 << 4;
			light.L = mef;
			gpu_lights.emplace_back(light);
			total_light_triangle_cnt += light.num_triangles;
		}
		idx++;
	}

	for (auto i = 0; i < lights.size(); i++) {
		auto& l = lights[i];
		Light light;
		light.L = l.L;
		light.light_flags = l.light_flags;
		light.pos = l.pos;
		light.to = l.to;
		total_light_triangle_cnt++;
		light.world_radius = m_dimensions.radius;
		light.world_center = 0.5f * (m_dimensions.max + m_dimensions.min);
		if ((l.light_flags & LIGHT_DIRECTIONAL) == LIGHT_DIRECTIONAL) {
			dir_light_idx = i;
		}
		gpu_lights.emplace_back(light);
	}

	float total_light_triangle_area = 0.0f;
	for (auto& l : gpu_lights) {
		if ((l.light_flags & 0x7) == LIGHT_AREA) {
			const auto& pm = prim_meshes[l.prim_mesh_idx];
			l.world_matrix = pm.world_matrix;
			auto& idx_base_offset = pm.first_idx;
			auto& vtx_offset = pm.vtx_offset;
			for (uint32_t i = 0; i < l.num_triangles; i++) {
				auto idx_offset = idx_base_offset + 3 * i;
				glm::ivec3 ind = {indices[idx_offset], indices[idx_offset + 1], indices[idx_offset + 2]};
				ind += glm::vec3{vtx_offset, vtx_offset, vtx_offset};
				const vec3 v0 = pm.world_matrix * glm::vec4(positions[ind.x], 1.0);
				const vec3 v1 = pm.world_matrix * glm::vec4(positions[ind.y], 1.0);
				const vec3 v2 = pm.world_matrix * glm::vec4(positions[ind.z], 1.0);
				float area = 0.5f * glm::length(glm::cross(v1 - v0, v2 - v0));
				total_light_triangle_area += area;
			}
		}
	}
	if (gpu_lights.size()) {
		// mesh_lights_buffer.create("Mesh Lights Buffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		// 						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gpu_lights.size() * sizeof(Light),
		// 						  gpu_lights.data(), true);

		mesh_lights_buffer = prm::get_buffer({.name = "Mesh Lights Buffer",
											  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
											  .memory_type = vk::BufferType::GPU,
											  .size = gpu_lights.size() * sizeof(Light),
											  .data = gpu_lights.data()});
	}
	total_light_area += total_light_triangle_area;
	vertex_buffer = prm::get_buffer({.name = "Vertex Buffer",
									 .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
											  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
									 .memory_type = vk::BufferType::GPU,
									 .size = positions.size() * sizeof(glm::vec3),
									 .data = positions.data()});

	index_buffer = prm::get_buffer({.name = "Index Buffer",
									.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
											 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
									.memory_type = vk::BufferType::GPU,
									.size = indices.size() * sizeof(uint32_t),
									.data = indices.data()});

	materials_buffer =
		prm::get_buffer({.name = "Materials Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = materials.size() * sizeof(Material),
						 .data = materials.data()});

	prim_lookup_buffer =
		prm::get_buffer({.name = "Prim Lookup Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = prim_lookup.size() * sizeof(PrimMeshInfo),
						 .data = prim_lookup.data()});

	std::vector<Vertex> vertices;
	vertices.reserve(positions.size());
	for (auto i = 0; i < positions.size(); i++) {
		Vertex v;
		v.pos = positions[i];
		v.normal = normals[i];
		v.uv0 = texcoords0[i];
		vertices.push_back(v);
	}
	compact_vertices_buffer =
		prm::get_buffer({.name = "Compact Vertices Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = vertices.size() * sizeof(vertices[0]),
						 .data = vertices.data()});

	// Create a sampler for textures
	VkSamplerCreateInfo sampler_ci = vk::sampler();
	sampler_ci.minFilter = VK_FILTER_LINEAR;
	sampler_ci.magFilter = VK_FILTER_LINEAR;
	sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_ci.maxLod = FLT_MAX;
	vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &texture_sampler));

	if (!textures.size()) {
		add_default_texture();
	} else {
		scene_textures.resize(textures.size());
		int i = 0;
		for (const auto& texture_path : textures) {
			int x, y, n;
			unsigned char* data = stbi_load(texture_path.c_str(), &x, &y, &n, 4);
			scene_textures[i] = prm::get_texture({.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												  .dimensions = {(uint32_t)x, (uint32_t)y, 1},
												  .format = VK_FORMAT_R8G8B8A8_SRGB,
												  .data = {.data = data, .size = size_t(x * y * 4)},
												  .sampler = texture_sampler});
			stbi_image_free(data);
			i++;
		}
	}
	vk::render_graph()->global_macro_defines.push_back(
		vk::ShaderMacro("ENABLE_DIFFUSE", has_bsdf_type(BSDF_TYPE_DIFFUSE), /* visible = */ false));
	vk::render_graph()->global_macro_defines.push_back(
		vk::ShaderMacro("ENABLE_MIRROR", has_bsdf_type(BSDF_TYPE_MIRROR), /* visible = */ false));
	vk::render_graph()->global_macro_defines.push_back(
		vk::ShaderMacro("ENABLE_GLASS", has_bsdf_type(BSDF_TYPE_GLASS), /* visible = */ false));
	vk::render_graph()->global_macro_defines.push_back(
		vk::ShaderMacro("ENABLE_DIELECTRIC", has_bsdf_type(BSDF_TYPE_DIELECTRIC), /* visible = */ false));
	vk::render_graph()->global_macro_defines.push_back(
		vk::ShaderMacro("ENABLE_CONDUCTOR", has_bsdf_type(BSDF_TYPE_CONDUCTOR), /* visible = */ false));
	vk::render_graph()->global_macro_defines.push_back(
		vk::ShaderMacro("ENABLE_PRINCIPLED", has_bsdf_type(BSDF_TYPE_PRINCIPLED), /* visible = */ false));
}

void LumenScene::load_lumen_scene(const std::string& path) {
	auto root = path.substr(0, path.find_last_of("/\\") + 1);
	std::ifstream i(path);
	json j;
	i >> j;

	auto& integrator = j["integrator"];
	create_scene_config(integrator["type"]);

	SceneConfig* curr_config = config.get();
	if (!integrator["path_length"].is_null()) {
		curr_config->path_length = integrator["path_length"];
	}
	if (!integrator["sky_col"].is_null()) {
		auto sky = integrator["sky_col"];
		curr_config->sky_col = glm::vec3(sky[0], sky[1], sky[2]);
	}
	if (integrator["type"] == "sppm") {
		((SPPMConfig*)curr_config)->base_radius = integrator["base_radius"];
	} else if (integrator["type"] == "vcm") {
		((VCMConfig*)curr_config)->enable_vm = integrator["enable_vm"] == 1;
		((VCMConfig*)curr_config)->radius_factor = integrator["radius_factor"];
	} else if (integrator["type"] == "pssmlt") {
		((PSSMLTConfig*)curr_config)->mutations_per_pixel = integrator["mutations_per_pixel"];
		((PSSMLTConfig*)curr_config)->num_mlt_threads = integrator["num_mlt_threads"];
		((PSSMLTConfig*)curr_config)->num_bootstrap_samples = integrator["num_bootstrap_samples"];
	} else if (integrator["type"] == "smlt") {
		((SMLTConfig*)curr_config)->mutations_per_pixel = integrator["mutations_per_pixel"];
		((SMLTConfig*)curr_config)->num_mlt_threads = integrator["num_mlt_threads"];
		((SMLTConfig*)curr_config)->num_bootstrap_samples = integrator["num_bootstrap_samples"];
	} else if (integrator["type"] == "vcmmlt") {
		((VCMMLTConfig*)curr_config)->mutations_per_pixel = integrator["mutations_per_pixel"];
		((VCMMLTConfig*)curr_config)->num_mlt_threads = integrator["num_mlt_threads"];
		((VCMMLTConfig*)curr_config)->num_bootstrap_samples = integrator["num_bootstrap_samples"];
		((VCMMLTConfig*)curr_config)->radius_factor = integrator["radius_factor"];
		((VCMMLTConfig*)curr_config)->enable_vm = integrator["enable_vm"] == 1;
		((VCMMLTConfig*)curr_config)->alternate = integrator["alternate"] == 1;
		((VCMMLTConfig*)curr_config)->light_first = integrator["light_first"] == 1;
	}
	// Load obj file
	const std::string mesh_file = root + std::string(j["mesh_file"]);
	tinyobj::ObjReaderConfig reader_config;

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(mesh_file, reader_config)) {
		if (!reader.Error().empty()) {
			std::cerr << "TinyObjReader: " << reader.Error();
		}
		exit(1);
	}

	if (!reader.Warning().empty()) {
		std::cout << "TinyObjReader: " << reader.Warning();
	}

	auto& attrib = reader.GetAttrib();
	auto& shapes = reader.GetShapes();

	prim_meshes.resize(shapes.size());
	for (uint32_t s = 0; s < shapes.size(); s++) {
		MeshData mesh_data;
		prim_meshes[s].first_idx = (uint32_t)indices.size();
		prim_meshes[s].vtx_offset = (uint32_t)positions.size();
		prim_meshes[s].name = shapes[s].name;
		prim_meshes[s].idx_count = (uint32_t)shapes[s].mesh.indices.size();
		prim_meshes[s].vtx_count = (uint32_t)shapes[s].mesh.num_face_vertices.size();
		prim_meshes[s].prim_idx = s;
		glm::vec3 min_vtx = glm::vec3(FLT_MAX);
		glm::vec3 max_vtx = glm::vec3(-FLT_MAX);
		uint32_t index_offset = 0;
		uint32_t idx_val = 0;
		for (uint32_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			for (uint32_t v = 0; v < 3; v++) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				mesh_data.indices.push_back(idx_val++);
				tinyobj::real_t vx = attrib.vertices[3 * uint32_t(idx.vertex_index) + 0];
				tinyobj::real_t vy = attrib.vertices[3 * uint32_t(idx.vertex_index) + 1];
				tinyobj::real_t vz = attrib.vertices[3 * uint32_t(idx.vertex_index) + 2];
				mesh_data.positions.emplace_back(vx, vy, vz);
				min_vtx = glm::min(mesh_data.positions[mesh_data.positions.size() - 1], min_vtx);
				max_vtx = glm::max(mesh_data.positions[mesh_data.positions.size() - 1], max_vtx);
				if (idx.normal_index >= 0) {
					tinyobj::real_t nx = attrib.normals[3 * uint32_t(idx.normal_index) + 0];
					tinyobj::real_t ny = attrib.normals[3 * uint32_t(idx.normal_index) + 1];
					tinyobj::real_t nz = attrib.normals[3 * uint32_t(idx.normal_index) + 2];
					mesh_data.normals.emplace_back(nx, ny, nz);
				}
				if (idx.texcoord_index >= 0) {
					tinyobj::real_t tx = attrib.texcoords[2 * uint32_t(idx.texcoord_index) + 0];
					tinyobj::real_t ty = attrib.texcoords[2 * uint32_t(idx.texcoord_index) + 1];
					mesh_data.texcoords0.emplace_back(tx, ty);
				}
			}
			index_offset += 3;
		}
		prim_meshes[s].min_pos = min_vtx;
		prim_meshes[s].max_pos = max_vtx;
		prim_meshes[s].world_matrix = glm::mat4(1);

		positions.insert(positions.end(), std::make_move_iterator(mesh_data.positions.begin()),
						 std::make_move_iterator(mesh_data.positions.end()));
		indices.insert(indices.end(), std::make_move_iterator(mesh_data.indices.begin()),
					   std::make_move_iterator(mesh_data.indices.end()));
		normals.insert(normals.end(), std::make_move_iterator(mesh_data.normals.begin()),
					   std::make_move_iterator(mesh_data.normals.end()));
		tangents.insert(tangents.end(), std::make_move_iterator(mesh_data.tangents.begin()),
						std::make_move_iterator(mesh_data.tangents.end()));
		texcoords0.insert(texcoords0.end(), std::make_move_iterator(mesh_data.texcoords0.begin()),
						  std::make_move_iterator(mesh_data.texcoords0.end()));
		texcoords1.insert(texcoords1.end(), std::make_move_iterator(mesh_data.texcoords1.begin()),
						  std::make_move_iterator(mesh_data.texcoords1.end()));
		colors0.insert(colors0.end(), std::make_move_iterator(mesh_data.colors0.begin()),
					   std::make_move_iterator(mesh_data.colors0.end()));
		// TODO: Implement world transforms
	}

	auto& bsdfs_arr = j["bsdfs"];
	auto& lights_arr = j["lights"];
	materials.resize(bsdfs_arr.size());
	lights.resize(lights_arr.size());
	int bsdf_idx = 0;
	int light_idx = 0;
	for (auto& bsdf : bsdfs_arr) {
		materials[bsdf_idx].texture_id = -1;
		auto& refs = bsdf["refs"];

		if (!bsdf["texture"].is_null()) {
			textures.push_back(root + (std::string)bsdf["texture"]);
			materials[bsdf_idx].texture_id = (int)textures.size() - 1;
		}
		if (!bsdf["albedo"].is_null()) {
			const auto& f = bsdf["albedo"];
			materials[bsdf_idx].albedo = glm::vec3({f[0], f[1], f[2]});
		} else {
			materials[bsdf_idx].albedo = glm::vec3(1, 1, 1);
		}
		if (!bsdf["emissive_factor"].is_null()) {
			const auto& f = bsdf["emissive_factor"];
			materials[bsdf_idx].emissive_factor = glm::vec3({f[0], f[1], f[2]});
		}

		if (bsdf["type"] == "diffuse") {
			bsdf_types |= BSDF_TYPE_DIFFUSE;
			materials[bsdf_idx].bsdf_type = BSDF_TYPE_DIFFUSE;
			materials[bsdf_idx].bsdf_props = BSDF_FLAG_DIFFUSE_REFLECTION;
		} else if (bsdf["type"] == "mirror") {
			bsdf_types |= BSDF_TYPE_MIRROR;
			materials[bsdf_idx].bsdf_type = BSDF_TYPE_MIRROR;
			materials[bsdf_idx].bsdf_props = BSDF_FLAG_SPECULAR_REFLECTION;
		} else if (bsdf["type"] == "glass") {
			bsdf_types |= BSDF_TYPE_GLASS;
			materials[bsdf_idx].bsdf_type = BSDF_TYPE_GLASS;
			materials[bsdf_idx].bsdf_props = BSDF_FLAG_SPECULAR_TRANSMISSION;
			materials[bsdf_idx].ior = bsdf["ior"];
		} else if (bsdf["type"] == "dielectric") {
			bsdf_types |= BSDF_TYPE_DIELECTRIC;
			materials[bsdf_idx].bsdf_type = BSDF_TYPE_DIELECTRIC;
			auto ior = bsdf["ior"];
			auto roughness = bsdf["roughness"];
			auto transmission = bsdf["transmission"];
			auto reflection = bsdf["reflection"];
			materials[bsdf_idx].ior = ior.is_null() ? 1.0f : float(ior);
			materials[bsdf_idx].roughness = roughness.is_null() ? 0.0f : float(roughness);
			if (transmission.is_null() || bool(transmission)) {
				materials[bsdf_idx].bsdf_props |= BSDF_FLAG_TRANSMISSION;
			}
			if (reflection.is_null() || bool(reflection)) {
				materials[bsdf_idx].bsdf_props |= BSDF_FLAG_REFLECTION;
			}
			if (materials[bsdf_idx].ior != 1.0 && materials[bsdf_idx].roughness > 0.08) {
				materials[bsdf_idx].bsdf_props |= BSDF_FLAG_GLOSSY;
			} else {
				materials[bsdf_idx].bsdf_props |= BSDF_FLAG_SPECULAR;
			}
			materials[bsdf_idx].thin = get_or_default_u(bsdf, "thin", 0);
		} else if (bsdf["type"] == "conductor") {
			bsdf_types |= BSDF_TYPE_CONDUCTOR;
			Material& mat = materials[bsdf_idx];
			mat.bsdf_type = BSDF_TYPE_CONDUCTOR;
			auto roughness = bsdf["roughness"];
			auto reflectance = bsdf["reflectance"];

			mat.roughness = roughness.is_null() ? 0.0f : float(roughness);

			// In conductor context, albedo is used as eta (i.e the IOR)
			// k is the absorption coefficient
			if (!reflectance.is_null()) {
				auto reflectance_val =
					glm::clamp(glm::vec3(reflectance[0], reflectance[1], reflectance[2]), 0.0f, 0.9999f);
				reflectance_to_conductor_eta_k(reflectance_val, mat.albedo, mat.k);
			}
			auto reflectivity = bsdf["reflectivity"];
			auto edge_tint = bsdf["edge_tint"];
			if (!edge_tint.is_null()) {
				// Apply the mappings from https://jcgt.org/published/0003/04/03/paper.pdf
				auto edge_tint_vec = glm::vec3{edge_tint[0], edge_tint[1], edge_tint[2]};
				auto reflectivity_vec = glm::vec3{reflectivity[0], reflectivity[1], reflectivity[2]};
				mat.albedo = edge_tint_vec * (1.0f - reflectivity_vec) / (1.0f + reflectivity_vec) +
							 (1.0f - edge_tint_vec) * (1.0f + glm::sqrt(reflectivity_vec)) /
								 (1.0f - glm::sqrt(reflectivity_vec));
				auto intermediate_term = reflectivity_vec * (mat.albedo + 1.0f);
				auto intermediate_term2 = mat.albedo - 1.0f;
				mat.k = glm::sqrt(1.0f / (1.0f - reflectivity_vec) *
								  (intermediate_term * intermediate_term - intermediate_term2 * intermediate_term2));
			}
			if (!reflectivity.is_null()) {
			}
			materials[bsdf_idx].bsdf_props = BSDF_FLAG_REFLECTION;
			if (materials[bsdf_idx].roughness > 0.08) {
				materials[bsdf_idx].bsdf_props |= BSDF_FLAG_GLOSSY;
			} else {
				materials[bsdf_idx].bsdf_props |= BSDF_FLAG_SPECULAR;
			}
		} else if (bsdf["type"] == "principled") {
			bsdf_types |= BSDF_TYPE_PRINCIPLED;
			Material& mat = materials[bsdf_idx];
			mat.bsdf_type = BSDF_TYPE_PRINCIPLED;
			mat.albedo = get_or_default_v(bsdf, "albedo", glm::vec3(1));
			mat.ior = get_or_default_f(bsdf, "ior", 1.0f);
			mat.roughness = get_or_default_f(bsdf, "roughness", 0.5f);
			mat.diffuse_trans = get_or_default_f(bsdf, "diffuse_transmission", 0.0f);
			mat.spec_trans = get_or_default_f(bsdf, "specular_transmission", 0.0f);
			mat.metallic = get_or_default_f(bsdf, "metallic", 0.0f);
			mat.specular_tint = get_or_default_f(bsdf, "specular_tint", 0.0f);
			mat.sheen_tint = get_or_default_f(bsdf, "sheen_tint", 0.5f);
			mat.clearcoat = get_or_default_f(bsdf, "clearcoat", 0.0f);
			mat.clearcoat_gloss = get_or_default_f(bsdf, "clearcoat_gloss", 1.0f);
			mat.subsurface = get_or_default_f(bsdf, "subsurface", 0.0f);
			mat.flatness = get_or_default_f(bsdf, "flatness", 0.0f);
			mat.sheen = get_or_default_f(bsdf, "sheen", 0.0f);
			mat.anisotropy = get_or_default_f(bsdf, "anisotropy", 0.0f);
			mat.thin = get_or_default_u(bsdf, "thin", 0);

			if (mat.roughness < 1.0f) {
				mat.bsdf_props |= BSDF_FLAG_REFLECTION;
			}
			if (mat.spec_trans > 0.0f) {
				mat.bsdf_props |= BSDF_FLAG_TRANSMISSION;
			}
			if (mat.roughness > 0.08) {
				mat.bsdf_props |= BSDF_FLAG_GLOSSY;
			} else {
				mat.bsdf_props |= BSDF_FLAG_SPECULAR;
			}
		}

		for (auto& ref : refs) {
			for (int s = 0; s < shapes.size(); s++) {
				if (ref == shapes[s].name) {
					prim_meshes[s].material_idx = bsdf_idx;
				}
			}
		}
		bsdf_idx++;
	}

	curr_config->cam_settings.fov = j["camera"]["fov"];
	const auto& p = j["camera"]["position"];
	const auto& d = j["camera"]["dir"];
	curr_config->cam_settings.pos = {p[0], p[1], p[2]};
	curr_config->cam_settings.dir = {d[0], d[1], d[2]};
	compute_scene_dimensions();
	for (auto& light : lights_arr) {
		const auto& pos = light["pos"];
		const auto& dir = light["dir"];
		const auto& L = light["L"];
		lights[light_idx].pos = glm::vec3({pos[0], pos[1], pos[2]});
		lights[light_idx].to = glm::vec3({dir[0], dir[1], dir[2]});
		lights[light_idx].L = glm::vec3({L[0], L[1], L[2]});
		if (light["type"] == "spot") {
			lights[light_idx].light_flags |= LIGHT_SPOT;
			// Is finite
			lights[light_idx].light_flags |= 1 << 4;
			// Is delta
			lights[light_idx].light_flags |= 1 << 5;
		} else if (light["type"] == "directional") {
			lights[light_idx].light_flags |= LIGHT_DIRECTIONAL;
			// Is delta
			lights[light_idx].light_flags |= 1 << 5;
		}
		light_idx++;
	}
}
void LumenScene::load_mitsuba_scene(const std::string& path) {
	auto root = path.substr(0, path.find_last_of("/\\") + 1);
	MitsubaParser mitsuba_parser;
	mitsuba_parser.parse(path);

	create_scene_config(mitsuba_parser.integrator.type);

	SceneConfig* curr_config = config.get();

	curr_config->path_length = mitsuba_parser.integrator.depth;
	curr_config->sky_col = mitsuba_parser.integrator.sky_col;
	// TODO: Introduce other config settings for Mitsuba format
	if (mitsuba_parser.integrator.type == "vcm") {
		((VCMConfig*)curr_config)->integrator_type = IntegratorType::VCM;
		((VCMConfig*)curr_config)->enable_vm = mitsuba_parser.integrator.enable_vm;
	}

	// Camera
	curr_config->cam_settings.fov = mitsuba_parser.camera.fov / 2;
	curr_config->cam_settings.cam_matrix = mitsuba_parser.camera.cam_matrix;
	prim_meshes.resize(mitsuba_parser.meshes.size());
	// Load objs
	int i = 0;
	for (const auto& mesh : mitsuba_parser.meshes) {
		if (mesh.file == "") {
			continue;
		}
		const std::string mesh_file = root + mesh.file;
		tinyobj::ObjReaderConfig reader_config;

		tinyobj::ObjReader reader;
		if (!reader.ParseFromFile(mesh_file, reader_config)) {
			if (!reader.Error().empty()) {
				std::cerr << "TinyObjReader: " << reader.Error();
			}
			exit(1);
		}

		if (!reader.Warning().empty()) {
			std::cout << "TinyObjReader: " << reader.Warning();
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		assert(shapes.size() == 1);
		prim_meshes[i].first_idx = (uint32_t)indices.size();
		prim_meshes[i].vtx_offset = (uint32_t)positions.size();
		prim_meshes[i].name = shapes[0].name;
		prim_meshes[i].idx_count = (uint32_t)shapes[0].mesh.indices.size();
		prim_meshes[i].vtx_count = (uint32_t)shapes[0].mesh.num_face_vertices.size();
		prim_meshes[i].prim_idx = i;
		glm::vec3 min_vtx = glm::vec3(FLT_MAX);
		glm::vec3 max_vtx = glm::vec3(-FLT_MAX);
		uint32_t index_offset = 0;
		uint32_t idx_val = 0;
		for (uint32_t f = 0; f < shapes[0].mesh.num_face_vertices.size(); f++) {
			for (uint32_t v = 0; v < 3; v++) {
				tinyobj::index_t idx = shapes[0].mesh.indices[index_offset + v];
				indices.push_back(idx_val++);
				tinyobj::real_t vx = attrib.vertices[3 * uint32_t(idx.vertex_index) + 0];
				tinyobj::real_t vy = attrib.vertices[3 * uint32_t(idx.vertex_index) + 1];
				tinyobj::real_t vz = attrib.vertices[3 * uint32_t(idx.vertex_index) + 2];
				positions.emplace_back(vx, vy, vz);
				min_vtx = glm::min(positions[positions.size() - 1], min_vtx);
				max_vtx = glm::max(positions[positions.size() - 1], max_vtx);
				if (idx.normal_index >= 0) {
					tinyobj::real_t nx = attrib.normals[3 * uint32_t(idx.normal_index) + 0];
					tinyobj::real_t ny = attrib.normals[3 * uint32_t(idx.normal_index) + 1];
					tinyobj::real_t nz = attrib.normals[3 * uint32_t(idx.normal_index) + 2];
					normals.emplace_back(nx, ny, nz);
				}
				if (idx.texcoord_index >= 0) {
					tinyobj::real_t tx = attrib.texcoords[2 * uint32_t(idx.texcoord_index) + 0];
					tinyobj::real_t ty = attrib.texcoords[2 * uint32_t(idx.texcoord_index) + 1];
					texcoords0.emplace_back(tx, ty);
				}
			}

			index_offset += 3;
		}
		prim_meshes[i].min_pos = min_vtx;
		prim_meshes[i].max_pos = max_vtx;
		prim_meshes[i].world_matrix = mesh.transform;
		prim_meshes[i].material_idx = mesh.bsdf_idx;
		i++;
	}

	auto make_default_principled = [](Material& m) {
		m.metallic = 0;
		m.specular_tint = 0;
		m.sheen_tint = 0.0;
		m.clearcoat = 0;
		m.clearcoat_gloss = 0;
		m.subsurface = 0;
		m.sheen = 0;
		m.thin = 0;
	};
	i = 0;
	materials.resize(mitsuba_parser.bsdfs.size());
	for (const auto& m_bsdf : mitsuba_parser.bsdfs) {
		if (m_bsdf.texture != "") {
			textures.push_back(root + m_bsdf.texture);
			materials[i].texture_id = (int)textures.size() - 1;
		} else {
			materials[i].texture_id = -1;
		}
		Material& mat = materials[i];
		make_default_principled(mat);
		mat.albedo = m_bsdf.albedo;
		mat.roughness = m_bsdf.roughness;
		// Assume Principled for other materials for now
		if (m_bsdf.type == "diffuse") {
			bsdf_types |= BSDF_TYPE_DIFFUSE;
			mat.bsdf_type = BSDF_TYPE_DIFFUSE;
			mat.bsdf_props = BSDF_FLAG_DIFFUSE_REFLECTION;
		} else if (m_bsdf.type == "roughplastic" || m_bsdf.type == "roughdielectric" || m_bsdf.type == "dielectric" ||
				   m_bsdf.type == "plastic") {
			bsdf_types |= BSDF_TYPE_PRINCIPLED;
			mat.bsdf_type = BSDF_TYPE_PRINCIPLED;
			mat.ior = m_bsdf.ior;
			if (mat.roughness < 1.0) {
				mat.bsdf_props |= BSDF_FLAG_DIFFUSE_REFLECTION;
			}
			if (mat.ior != 1.0) {
				mat.bsdf_props |= BSDF_FLAG_TRANSMISSION;
			}
			if (mat.roughness > 0.08) {
				mat.bsdf_props |= BSDF_FLAG_GLOSSY;
			} else {
				mat.bsdf_props |= BSDF_FLAG_SPECULAR;
			}
			if (m_bsdf.type == "roughdielectric" || m_bsdf.type == "dielectric") {
				mat.spec_trans = 1.0;
				mat.metallic = 0.0;
			}
			if (m_bsdf.type == "roughplastic" || m_bsdf.type == "plastic") {
				// The roughplastic in Mitsuba is not compatible with the Disney principled model
				mat.metallic = 1.0;
				mat.subsurface = 0.1f;
				mat.spec_trans = 0.5;
				mat.thin = 1;
			}

		} else if (m_bsdf.type == "conductor" || m_bsdf.type == "roughconductor") {
			bsdf_types |= BSDF_TYPE_CONDUCTOR;
			mat.bsdf_type = BSDF_TYPE_CONDUCTOR;
			reflectance_to_conductor_eta_k(m_bsdf.albedo, mat.albedo, mat.k);
			mat.bsdf_props = BSDF_FLAG_REFLECTION;
			if (mat.roughness > 0.08) {
				mat.bsdf_props |= BSDF_FLAG_GLOSSY;
			} else {
				mat.bsdf_props |= BSDF_FLAG_SPECULAR;
			}
		} else if (m_bsdf.type == "glass") {
			bsdf_types |= BSDF_TYPE_GLASS;
			mat.bsdf_type = BSDF_TYPE_GLASS;
			mat.bsdf_props = BSDF_FLAG_SPECULAR_TRANSMISSION;
			mat.ior = m_bsdf.ior;
		}
		i++;
	}
	compute_scene_dimensions();
	// Light
	i = 0;
	lights.resize(mitsuba_parser.lights.size());
	for (auto& light : mitsuba_parser.lights) {
		lights[i].L = 100.0f * light.L;
		if (light.type == "directional") {
			lights[i].pos = light.from;
			lights[i].to = light.to;
			lights[i].light_flags = LIGHT_DIRECTIONAL;
			// Is delta
			lights[i].light_flags |= 1 << 5;
		}
		i++;
	}
}

void LumenScene::add_default_texture() {
	std::array<uint8_t, 4> nil = {0, 0, 0, 0};
	scene_textures.resize(1);
	scene_textures[0] = prm::get_texture({.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
										  .dimensions = {1, 1, 1},
										  .format = VK_FORMAT_R8G8B8A8_SRGB,
										  .data = {.data = nil.data(), .size = 4},
										  .sampler = texture_sampler});
}

void LumenScene::create_scene_config(const std::string& integrator_name) {
	std::string name = integrator_name;
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
	if (name == "path") {
		config = std::make_unique<PathConfig>();
	} else if (name == "bdpt") {
		config = std::make_unique<BDPTConfig>();
	} else if (name == "sppm") {
		config = std::make_unique<SPPMConfig>();
	} else if (name == "vcm") {
		config = std::make_unique<VCMConfig>();
	} else if (name == "pssmlt") {
		config = std::make_unique<PSSMLTConfig>();
	} else if (name == "smlt") {
		config = std::make_unique<SMLTConfig>();
	} else if (name == "vcmmlt") {
		config = std::make_unique<VCMMLTConfig>();
	} else if (name == "restir") {
		config = std::make_unique<ReSTIRConfig>();
	} else if (name == "restirgi") {
		config = std::make_unique<ReSTIRGIConfig>();
	} else if (name == "restirpt") {
		config = std::make_unique<ReSTIRPTConfig>();
	} else if (name == "ddgi") {
		config = std::make_unique<DDGIConfig>();
	} else {
		config = std::make_unique<PathConfig>();
	}
}

void LumenScene::compute_scene_dimensions() {
	Bbox scene_bbox;
	for (const auto& pm : prim_meshes) {
		Bbox bbox(pm.min_pos, pm.max_pos);
		bbox.transform(pm.world_matrix);
		scene_bbox.insert(bbox);
	}
	if (scene_bbox.is_empty() || !scene_bbox.isVolume()) {
		LUMEN_WARN(
			"glTF: Scene bounding box invalid, Setting to: [-1,-1,-1], "
			"[1,1,1]");
		scene_bbox.insert({-1.0f, -1.0f, -1.0f});
		scene_bbox.insert({1.0f, 1.0f, 1.0f});
	}
	m_dimensions.min = scene_bbox.min();
	m_dimensions.max = scene_bbox.max();
	m_dimensions.size = scene_bbox.extents();
	m_dimensions.center = scene_bbox.center();
	m_dimensions.radius = scene_bbox.radius();
}

void LumenScene::destroy() {
	std::vector<vk::Buffer*> buffer_list = {index_buffer, vertex_buffer, compact_vertices_buffer, materials_buffer,
											prim_lookup_buffer};
	if (gpu_lights.size()) {
		buffer_list.push_back(mesh_lights_buffer);
	}
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
	for (vk::Texture* tex : scene_textures) {
		prm::remove(tex);
	}
	vkDestroySampler(vk::context().device, texture_sampler, nullptr);
}
