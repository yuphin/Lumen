#include "LumenPCH.h"
#include "LumenScene.h"
#include <json.hpp>
#include <tiny_obj_loader.h>
using json = nlohmann::json;
void LumenScene::load_scene(const std::string& root, const std::string& filename) {
	std::ifstream i(root + filename);
	json j;
	i >> j;

	// Load obj file
	const std::string mesh_file = root + std::string(j["mesh_file"]);
	tinyobj::ObjReaderConfig reader_config;
	//reader_config.mtl_search_path = "./"; // Path to material files

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
	materials.resize(shapes.size());
	positions.resize(attrib.vertices.size() / 3);
	normals.resize(attrib.vertices.size() / 3);
	texcoords0.resize(attrib.texcoords.size() / 2);
	for (size_t s = 0; s < shapes.size(); s++) {
		prim_meshes[s].first_idx = indices.size();
		prim_meshes[s].vtx_offset = positions.size();
		prim_meshes[s].name = shapes[s].name;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			size_t index_offset = 0;
			for (size_t v = 0; v < 3; v++) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
				tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
				tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
				positions.push_back({ vx,vy,vz });
				if (idx.normal_index >= 0) {
					tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
					tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
					tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
					normals.push_back({ nx,ny,nz });
				}
				if (idx.texcoord_index >= 0) {
					tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
					tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
					texcoords0.push_back({ tx,ty });
				}
			}
			index_offset += 3;
			// per-face material
			shapes[s].mesh.material_ids[f];
		}
		prim_meshes[s].world_matrix = glm::mat4(1);
		// TODO: Implement world transforms


	}
	auto& bsdfs_arr = j["bsdfs"];
	int bsdf_idx = 0;
	for (auto& bsdf : bsdfs_arr) {
		auto& refs = bsdf["refs"];
		if (!bsdf["albedo"].is_null()) {
			const auto& f = bsdf["albedo"];
			materials[bsdf_idx].albedo = glm::vec3({f[0], f[1], f[2]});
		}
		if (!bsdf["emissive_factor"].is_null()) {
			const auto& f = bsdf["emissive_factor"];
			materials[bsdf_idx].emissive_factor = glm::vec3({ f[0], f[1], f[2] });
		}

		for (auto& ref : refs) {
			for (int s = 0; s < shapes.size(); s++) {
				if (ref == shapes[s].name) {
					prim_meshes[s].material_idx = bsdf_idx;
				}
			}
			bsdf_idx++;
		}
	}
	cam_config.fov = j["camera"]["fov"];
	const auto& p = j["camera"]["position"];
	cam_config.pos = { p[0], p[1], p[2] };

}
