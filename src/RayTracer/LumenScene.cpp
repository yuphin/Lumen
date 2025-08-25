#include "Framework/BBox.h"
#include "LumenScene.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include "shaders/commons.h"
#include "Framework/PersistentResourceManager.h"
#include "Framework/Base/HashMap.h"

// TODO: Add instancing to the scene format
// TODO: Replace tiny_obj_loader

static lm::String str_from_std_string(lm::Arena* arena, const std::string& str) {
	lm::String result = {};
	if (str.empty()) {
		return result;
	}
	result.size = str.size() - 1;
	result.data = (char*)arena->allocate(result.size);
	memcpy(result.data, str.data(), str.size() - 1);
	return result;
}

static bool ends_with(const std::string& str, const std::string& end) {
	if (end.size() > str.size()) return false;
	return std::equal(end.rbegin(), end.rend(), str.rbegin());
}

static void reflectance_to_conductor_eta_k(const glm::vec3& reflectance, glm::vec3& eta, glm::vec3& k) {
	eta = glm::vec3(1.0f);
	k = 2.0f * glm::sqrt(reflectance) / glm::sqrt(glm::max(glm::vec3(1.0f) - reflectance, 0.001f));
};

static void insert_child(LumenNode* parent, LumenNode* child) {
	if (!parent->child) {
		parent->child = child;
	} else {
		LumenNode* sibling = parent->child;
		while (sibling->next) {
			sibling = sibling->next;
		}
		sibling->next = child;
	}
}
static LumenNode* parse_scene(lm::Arena* arena, lm::String buffer) {
	buffer[buffer.size - 1] = '\n';
	LumenNode* root = new LumenNode();
	root->key = "root";
	LumenNode* stack[64];

	u64 curr_line_idx = 0;

	bool can_span_multiple_lines = false;
	i32 base_indentation = 0;
	i32 stack_size = 0;
	u64 buffer_cursor = 0;
	while (buffer_cursor < buffer.size) {
		// Note: Currently only braces are allowed for this
		if (buffer[buffer_cursor] == '[') {
			can_span_multiple_lines = true;
		}
		u64 line_end_idx = U64_MAX;
#if defined(_WIN32) || defined(_WIN64)
		// Handle newline
		if (!can_span_multiple_lines && buffer[buffer_cursor] == '\r') {
			line_end_idx = buffer_cursor++;
		}
#endif
		if (buffer[buffer_cursor] == '\n') {
			if (!can_span_multiple_lines) {
				line_end_idx = std::min(line_end_idx, buffer_cursor);
			}
			u64 line_cursor = curr_line_idx;
			i32 indentation = 0;
			bool list_item = false;
			while (lm::char_is_whitespace(buffer[line_cursor])) {
				if (buffer[line_cursor] == '-') {
					list_item = true;
				}
				line_cursor++;
				indentation++;
			}
			if (line_cursor >= line_end_idx) {
				return nullptr;
			}
			if (base_indentation == 0) {
				base_indentation = indentation;
			}
			i32 level = base_indentation == 0 ? 0 : indentation / base_indentation;
			if (level < stack_size) {
				// We went to an outer scope, pop the stack
				stack_size = level;
			}
			if (list_item) {
				stack_size = level - 1;
				LumenNode* temp = (LumenNode*)arena->allocate(sizeof(LumenNode));
				stack[stack_size++] = temp;
				LUMEN_ASSERT(stack_size >= 2, "LumenNode list item level must be at least 2");
				temp->key = "-";
				temp->parent = stack[stack_size - 2];
				insert_child(stack[stack_size - 2], temp);
			}
			LumenNode* node = (LumenNode*)arena->allocate(sizeof(LumenNode));
			LumenNode* prev = level > 0 ? stack[stack_size - 1] : root;
			node->parent = prev;

			u64 colon_idx = line_cursor;
			while (buffer[colon_idx] != ':') colon_idx++;
			if (colon_idx >= line_end_idx) {
				return nullptr;
			}
			node->key = lm::str_substr(buffer, line_cursor, colon_idx - line_cursor);
			line_cursor = colon_idx + 1;
			while (lm::char_is_whitespace(buffer[line_cursor])) line_cursor++;
			assert(line_cursor < line_end_idx);
			if (buffer[line_cursor] == '[') {
				++line_cursor;
				u64 list_cursor = line_cursor;
				while (lm::char_is_whitespace(buffer[line_cursor])) line_cursor++;
				node->num_list_items = 1;
				for (; buffer[line_cursor] != ']'; line_cursor++) {
					if (line_cursor >= line_end_idx) {
						return nullptr;
					}
					if (buffer[line_cursor] == ',') {
						++node->num_list_items;
					}
				}
				node->value = lm::str_substr(buffer, list_cursor, line_cursor - list_cursor);
			} else {
				node->value = lm::str_substr(buffer, line_cursor, line_end_idx - line_cursor);
			}

			if (prev) {
				insert_child(prev, node);
			}
			LUMEN_ASSERT(stack_size < 64, "LumenNode stack overflow");
			stack[stack_size++] = node;
			can_span_multiple_lines = false;
		}
		++buffer_cursor;
	}

	return root;
}

static LumenNode* get_node(LumenNode* node, const lm::String& name) {
	if (!node) return nullptr;
	if (node->key == name) return node;
	for (LumenNode* child = node->child; child; child = child->next) {
		LumenNode* found = get_node(child, name);
		if (found) return found;
	}
	return nullptr;
}

static lm::String get_str(LumenNode* node) {
	assert(node);
	return node->value;
}
static lm::String get_or_default_str(LumenNode* node, const lm::String& val) {
	if (!node || node->value.empty()) {
		return val;
	}
	return node->value;
}
static i32 get_or_default_i(LumenNode* node, i32 val) {
	if (!node || node->value.empty()) {
		return val;
	}
	return std::atoi(node->value.data);
}

static f32 get_or_default_f(LumenNode* node, f32 val) {
	if (!node || node->value.empty()) {
		return val;
	}
	return (f32)std::atof(node->value.data);
}

static lm::Array<lm::String> get_str_list(lm::Arena* arena, LumenNode* node) {
	if (!node || node->value.empty()) {
		return {};
	}
	assert(node->num_list_items);
	lm::Array<lm::String> result = lm::array_create<lm::String>(arena);
	result.reserve(node->num_list_items);
	char* ptr = node->value.data;
	while (*ptr) {
		while (lm::char_is_whitespace(*ptr)) {
			ptr++;
		}
		char* start = ptr;
		while (*ptr != ',' && *ptr) {
			ptr++;
		}
		char* end = ptr - 1;
		while (lm::char_is_whitespace(*end)) {
			--end;
		}
		result.emplace_back(start, end - start + 1);
		if (*ptr == ',') {
			ptr++;
		}
	}
	return result;
}

static lm::String get_light_type_str(const LumenLight& light) {
	if (light.light_flags & LIGHT_SPOT) {
		return "spot";
	} else if (light.light_flags & LIGHT_DIRECTIONAL) {
		return "directional";
	}
	LUMEN_ASSERT(false, "Unknown light type");
	return "";
}

static glm::vec3 get_or_default_v3(LumenNode* node, const glm::vec3& val) {
	if (!node || node->value.empty()) {
		return val;
	}
	glm::vec3 result;

	char* ptr = node->value.data;
	while (*ptr && *ptr != '(') {
		ptr++;
	}
	i32 comma_count = 0;
	char* start = ++ptr;
	while (*ptr && *ptr != ')') {
		if (*ptr == ',') {
			std::string val = std::string(start, ptr - start);
			// atof handles the trailing spaces and other degeneracies
			result[comma_count++] = (f32)std::atof(val.data());
			start = ptr + 1;
		}
		ptr++;
	}
	LUMEN_ASSERT(comma_count == 2, "Expected 3 items in vec3");
	result[comma_count++] = (f32)std::atof(start);
	return result;
}

static LumenNode* next_node(LumenNode* node) {
	if (!node) return nullptr;
	if (node->next) return node->next;
	return nullptr;
}

static u32 get_child_count(LumenNode* node) {
	if (!node || !node->child) return 0;
	u32 count = 0;
	LumenNode* child = node->child;
	while (child) {
		count++;
		child = child->next;
	}
	return count;
}

void LumenScene::parse_lumen_scene(const lm::String& path, const lm::String& path_root, LumenNode* root) {
	LumenNode* integrator_node = get_node(root, "integrator");
	LumenNode* bsdfs_node = get_node(root, "bsdfs");
	LumenNode* camera_node = get_node(root, "camera");
	LumenNode* lights_node = get_node(root, "lights");

	lm::String integrator_type = get_or_default_str(get_node(integrator_node, "type"), "path");

	// TODO: Rework this function and return a pointer
	create_scene_config(integrator_type);
	SceneConfig* curr_config = config.get();

	curr_config->path_length = get_or_default_i(get_node(integrator_node, "path_length"), 6);
	curr_config->sky_col = get_or_default_v3(get_node(integrator_node, "sky_col"), glm::vec3(0.0f));
	if (integrator_type == "sppm") {
		((SPPMConfig*)curr_config)->base_radius = get_or_default_f(get_node(integrator_node, "base_radius"), 1);
	} else if (integrator_type == "vcm") {
		((VCMConfig*)curr_config)->enable_vm = get_or_default_i(get_node(integrator_node, "enable_vm"), 0) == 1;
		((VCMConfig*)curr_config)->radius_factor = get_or_default_f(get_node(integrator_node, "radius_factor"), 1);
	} else if (integrator_type == "pssmlt") {
		((PSSMLTConfig*)curr_config)->mutations_per_pixel =
			get_or_default_f(get_node(integrator_node, "mutations_per_pixel"), 0);
		((PSSMLTConfig*)curr_config)->num_mlt_threads =
			get_or_default_i(get_node(integrator_node, "num_mlt_threads"), 0);
		((PSSMLTConfig*)curr_config)->num_bootstrap_samples =
			get_or_default_i(get_node(integrator_node, "num_bootstrap_samples"), 0);
	} else if (integrator_type == "smlt") {
		((SMLTConfig*)curr_config)->mutations_per_pixel =
			get_or_default_f(get_node(integrator_node, "mutations_per_pixel"), 0);
		((SMLTConfig*)curr_config)->num_mlt_threads = get_or_default_i(get_node(integrator_node, "num_mlt_threads"), 0);
		((SMLTConfig*)curr_config)->num_bootstrap_samples =
			get_or_default_i(get_node(integrator_node, "num_bootstrap_samples"), 0);
	} else if (integrator_type == "vcmmlt") {
		((VCMMLTConfig*)curr_config)->mutations_per_pixel =
			get_or_default_f(get_node(integrator_node, "mutations_per_pixel"), 0);
		((VCMMLTConfig*)curr_config)->num_mlt_threads =
			get_or_default_i(get_node(integrator_node, "num_mlt_threads"), 0);
		((VCMMLTConfig*)curr_config)->num_bootstrap_samples =
			get_or_default_i(get_node(integrator_node, "num_bootstrap_samples"), 0);
		((VCMMLTConfig*)curr_config)->radius_factor = get_or_default_f(get_node(integrator_node, "radius_factor"), 1);
		((VCMMLTConfig*)curr_config)->enable_vm = get_or_default_i(get_node(integrator_node, "enable_vm"), 0) == 1;
		((VCMMLTConfig*)curr_config)->alternate = get_or_default_i(get_node(integrator_node, "alternate"), 0) == 1;
		((VCMMLTConfig*)curr_config)->light_first = get_or_default_i(get_node(integrator_node, "light_first"), 0) == 1;
	}

	materials.resize(get_child_count(bsdfs_node));

	// TODO: Revise hashmap logic altogether and initialize material_idx_to_name
	// TODO: Revise whether we need scratch arena or permanent
	lm::HashMap<lm::String, u32> material_map = lm::hash_map_create<lm::String, u32>(lm::arena());
	lm::HashMap<lm::String, u32> materials_to_objects = lm::hash_map_create<lm::String, u32>(lm::arena());
	lm::HashMap<lm::String, u32> texture_name_to_idx = lm::hash_map_create<lm::String, u32>(lm::arena());

	LumenNode* textures_node = get_node(root, "textures");

	if (textures_node) {
		for (LumenNode* texture_node = textures_node->child; texture_node; texture_node = next_node(texture_node)) {
			lm::String name = get_str(get_node(texture_node, "name"));
			lm::String file = get_str(get_node(texture_node, "file"));
			LUMEN_ASSERT(!name.empty() && !file.empty(), "Texture name and file must be specified");
			TextureRef ref;
			ref.name = name;
			ref.relative_path = file;
			textures.push_back(ref);
			u32 idx = (u32)textures.size() - 1;
			texture_name_to_idx.insert(name, idx);
		}
	}

	if (bsdfs_node) {
		u32 bsdf_idx = 0;
		for (LumenNode* bsdf_node = bsdfs_node->child; bsdf_node; bsdf_node = next_node(bsdf_node), bsdf_idx++) {
			materials[bsdf_idx].albedo = get_or_default_v3(get_node(bsdf_node, "albedo"), glm::vec3(1.0f));
			materials[bsdf_idx].emissive_factor =
				get_or_default_v3(get_node(bsdf_node, "emissive_factor"), glm::vec3(0.0f));
			materials[bsdf_idx].texture_id = -1;
			lm::String mat_name = get_str(get_node(bsdf_node, "name"));
			if (!mat_name.empty()) {
				material_map.insert(mat_name, bsdf_idx);
				material_idx_to_name.insert(bsdf_idx, mat_name);
			}

			lm::String texture_name = get_or_default_str(get_node(bsdf_node, "texture"), "");
			if (!texture_name.empty()) {
				auto entry = texture_name_to_idx.find(texture_name);
				if (entry) {
					materials[bsdf_idx].texture_id = entry->value;
				}
			}

			lm::String type = get_or_default_str(get_node(bsdf_node, "type"), "diffuse");
			if (type == "diffuse") {
				bsdf_types |= BSDF_TYPE_DIFFUSE;
				materials[bsdf_idx].bsdf_type = BSDF_TYPE_DIFFUSE;
				materials[bsdf_idx].bsdf_props = BSDF_FLAG_DIFFUSE_REFLECTION;
			} else if (type == "mirror") {
				bsdf_types |= BSDF_TYPE_MIRROR;
				materials[bsdf_idx].bsdf_type = BSDF_TYPE_MIRROR;
				materials[bsdf_idx].bsdf_props = BSDF_FLAG_SPECULAR_REFLECTION;
			} else if (type == "glass") {
				bsdf_types |= BSDF_TYPE_GLASS;
				materials[bsdf_idx].bsdf_type = BSDF_TYPE_GLASS;
				materials[bsdf_idx].bsdf_props = BSDF_FLAG_SPECULAR_TRANSMISSION;
				materials[bsdf_idx].ior = get_or_default_f(get_node(bsdf_node, "ior"), 1.0f);
			} else if (type == "dielectric") {
				bsdf_types |= BSDF_TYPE_DIELECTRIC;
				materials[bsdf_idx].bsdf_type = BSDF_TYPE_DIELECTRIC;

				materials[bsdf_idx].ior = get_or_default_f(get_node(bsdf_node, "ior"), 1.0f);
				materials[bsdf_idx].roughness = get_or_default_f(get_node(bsdf_node, "roughness"), 0.0f);

				LumenNode* transmission_node = get_node(bsdf_node, "transmission");
				LumenNode* reflection_node = get_node(bsdf_node, "reflection");
				bool transmission = !transmission_node || get_or_default_i(transmission_node, 1);
				bool reflection = !reflection_node || get_or_default_i(reflection_node, 1);
				if (transmission) {
					materials[bsdf_idx].bsdf_props |= BSDF_FLAG_TRANSMISSION;
				}
				if (reflection) {
					materials[bsdf_idx].bsdf_props |= BSDF_FLAG_REFLECTION;
				}
				if (materials[bsdf_idx].ior != 1.0f && materials[bsdf_idx].roughness > 0.08f) {
					materials[bsdf_idx].bsdf_props |= BSDF_FLAG_GLOSSY;
				} else {
					materials[bsdf_idx].bsdf_props |= BSDF_FLAG_SPECULAR;
				}
				materials[bsdf_idx].thin = get_or_default_i(get_node(bsdf_node, "thin"), 0);
			} else if (type == "conductor") {
				bsdf_types |= BSDF_TYPE_CONDUCTOR;
				Material& mat = materials[bsdf_idx];
				mat.bsdf_type = BSDF_TYPE_CONDUCTOR;
				mat.roughness = get_or_default_f(get_node(bsdf_node, "roughness"), 0.0f);

				// In conductor context, albedo is used as eta (i.e the IOR)
				// k is the absorption coefficient
				LumenNode* reflectance = get_node(bsdf_node, "reflectance");
				if (reflectance) {
					glm::vec3 reflectance_val =
						glm::clamp(get_or_default_v3(reflectance, glm::vec3(1.0f)), 0.0f, 0.9999f);
					reflectance_to_conductor_eta_k(reflectance_val, mat.albedo, mat.k);
				}

				// Apply the mappings from https://jcgt.org/published/0003/04/03/paper.pdf
				LumenNode* edge_tint = get_node(bsdf_node, "edge_tint");
				LumenNode* reflectivity = get_node(bsdf_node, "reflectivity");
				if (edge_tint && reflectivity) {
					glm::vec3 edge_tint_vec = get_or_default_v3(edge_tint, glm::vec3(1));
					glm::vec3 reflectivity_vec = get_or_default_v3(reflectivity, glm::vec3(1));
					mat.albedo = edge_tint_vec * (1.0f - reflectivity_vec) / (1.0f + reflectivity_vec) +
								 (1.0f - edge_tint_vec) * (1.0f + glm::sqrt(reflectivity_vec)) /
									 (1.0f - glm::sqrt(reflectivity_vec));
					auto intermediate_term = mat.albedo + 1.0f;
					auto intermediate_term2 = mat.albedo - 1.0f;
					mat.k = glm::sqrt(1.0f / (1.0f - reflectivity_vec) *
									  (reflectivity_vec * intermediate_term * intermediate_term -
									   intermediate_term2 * intermediate_term2));
				}

				mat.bsdf_props = BSDF_FLAG_REFLECTION;
				if (mat.roughness > 0.08f) {
					mat.bsdf_props |= BSDF_FLAG_GLOSSY;
				} else {
					mat.bsdf_props |= BSDF_FLAG_SPECULAR;
				}
			} else if (type == "principled") {
				bsdf_types |= BSDF_TYPE_PRINCIPLED;
				Material& mat = materials[bsdf_idx];
				mat.bsdf_type = BSDF_TYPE_PRINCIPLED;
				mat.albedo = get_or_default_v3(get_node(bsdf_node, "albedo"), glm::vec3(1));
				mat.ior = get_or_default_f(get_node(bsdf_node, "ior"), 1.0f);
				mat.roughness = get_or_default_f(get_node(bsdf_node, "roughness"), 0.5f);
				mat.diffuse_trans = get_or_default_f(get_node(bsdf_node, "diffuse_transmission"), 0.0f);
				mat.spec_trans = get_or_default_f(get_node(bsdf_node, "specular_transmission"), 0.0f);
				mat.metallic = get_or_default_f(get_node(bsdf_node, "metallic"), 0.0f);
				mat.specular_tint = get_or_default_f(get_node(bsdf_node, "specular_tint"), 0.0f);
				mat.sheen_tint = get_or_default_f(get_node(bsdf_node, "sheen_tint"), 0.5f);
				mat.clearcoat = get_or_default_f(get_node(bsdf_node, "clearcoat"), 0.0f);
				mat.clearcoat_gloss = get_or_default_f(get_node(bsdf_node, "clearcoat_gloss"), 1.0f);
				mat.subsurface = get_or_default_f(get_node(bsdf_node, "subsurface"), 0.0f);
				mat.flatness = get_or_default_f(get_node(bsdf_node, "flatness"), 0.0f);
				mat.sheen = get_or_default_f(get_node(bsdf_node, "sheen"), 0.0f);
				mat.anisotropy = get_or_default_f(get_node(bsdf_node, "anisotropy"), 0.0f);
				mat.thin = get_or_default_i(get_node(bsdf_node, "thin"), 0);

				if (mat.roughness < 1.0f) {
					mat.bsdf_props |= BSDF_FLAG_REFLECTION;
				}
				if (mat.spec_trans > 0.0f) {
					mat.bsdf_props |= BSDF_FLAG_TRANSMISSION;
				}
				if (mat.roughness > 0.08f) {
					mat.bsdf_props |= BSDF_FLAG_GLOSSY;
				} else {
					mat.bsdf_props |= BSDF_FLAG_SPECULAR;
				}
			}
		}
	}

	if (lights_node) {
		u32 light_idx = 0;
		lights.resize(get_child_count(lights_node));
		for (LumenNode* light_node = lights_node->child; light_node; light_node = next_node(light_node), light_idx++) {
			lm::String type = get_str(get_node(light_node, "type"));
			lights[light_idx].L = get_or_default_v3(get_node(light_node, "L"), glm::vec3(0));
			lights[light_idx].pos = get_or_default_v3(get_node(light_node, "pos"), glm::vec3(0));
			lights[light_idx].to = get_or_default_v3(get_node(light_node, "dir"), glm::vec3(0, 0, 1));
			if (type == "spot") {
				lights[light_idx].light_flags |= LIGHT_SPOT;
				// Is finite
				lights[light_idx].light_flags |= 1 << 4;
				// Is delta
				lights[light_idx].light_flags |= 1 << 5;
			} else if (type == "directional") {
				lights[light_idx].light_flags |= LIGHT_DIRECTIONAL;
				// Is delta
				lights[light_idx].light_flags |= 1 << 5;
			}
		}
	}

	LumenNode* meshes_node = get_node(root, "mesh");
	LUMEN_ASSERT(meshes_node, "Meshes node not found in Lumen scene");
	u32 mesh_idx = 0;
	for (LumenNode* mesh_node = meshes_node->child; mesh_node; mesh_node = next_node(mesh_node), mesh_idx++) {
		const lm::String relative_mesh_file = get_or_default_str(get_node(mesh_node, "name"), "");
		const lm::String mesh_file = lm::str_concat(lm::arena(), path_root, relative_mesh_file);
		LumenNode* materials_refs_node = get_node(mesh_node, "materials");
		LumenNode* transforms_node = get_node(mesh_node, "transforms");
		u32 material_entire_mesh_idx = U32_MAX;
		if (materials_refs_node) {
			for (LumenNode* mat_ref_node = materials_refs_node->child; mat_ref_node;
				 mat_ref_node = next_node(mat_ref_node)) {
				lm::String mat_name = get_str(get_node(mat_ref_node, "name"));
				auto material_entry = material_map.find(mat_name);
				if (material_entry) {
					u32 mat_idx = material_entry->value;
					LumenNode* refs_node = get_node(mat_ref_node, "refs");
					// std::vector<std::string> refs;
					lm::Array<lm::String> refs = lm::array_create<lm::String>(lm::arena());
					if (refs_node && refs_node->num_list_items) {
						refs = get_str_list(lm::arena(), get_node(mat_ref_node, "refs"));
					} else if (refs_node) {
						refs.push_back(get_str(refs_node));
					} else {
						// Material ref points to all the meshes in this file
						material_entire_mesh_idx = mat_idx;
						break;
					}
					for (const lm::String& ref : refs) {
						materials_to_objects.insert(ref, mat_idx);
					}
				} else {
					LUMEN_ERROR("Material {} not found in Lumen scene", mat_name.data);
				}
			}
		}
		glm::mat4 world_matrix = glm::mat4(1);
		if (transforms_node) {
			glm::vec3 translation = get_or_default_v3(get_node(transforms_node, "translation"), glm::vec3(0));
			glm::vec3 rotation = glm::radians(get_or_default_v3(get_node(transforms_node, "rotation"), glm::vec3(0)));
			glm::vec3 scale = get_or_default_v3(get_node(transforms_node, "scale"), glm::vec3(1));
			world_matrix = glm::translate(world_matrix, translation);
			world_matrix = glm::rotate(world_matrix, rotation.x, glm::vec3(1, 0, 0));
			world_matrix = glm::rotate(world_matrix, rotation.y, glm::vec3(0, 1, 0));
			world_matrix = glm::rotate(world_matrix, rotation.z, glm::vec3(0, 0, 1));
			world_matrix = glm::scale(world_matrix, scale);
		}
		// Load obj file
		tinyobj::ObjReaderConfig reader_config;

		tinyobj::ObjReader reader;
		lm::String mesh_file_cstr = lm::str_to_cstr(lm::arena(), mesh_file);
		if (!reader.ParseFromFile(mesh_file_cstr.data, reader_config)) {
			if (!reader.Error().empty()) {
				LUMEN_ERROR("Failed to load Lumen scene mesh file: {}", mesh_file_cstr.data);
			}
		}

		if (!reader.Warning().empty()) {
			std::cout << "TinyObjReader: " << reader.Warning();
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();

		for (u32 shape_idx = 0; shape_idx < shapes.size(); shape_idx++) {
			LumenPrimMesh& prim_mesh = prim_meshes.emplace_back();
			prim_mesh.name = str_from_std_string(lm::arena(), shapes[shape_idx].name);
			prim_mesh.filename = relative_mesh_file;
			prim_mesh.vtx_offset = (u32)positions.size();
			prim_mesh.first_idx = (u32)indices.size();
			prim_mesh.idx_count = (u32)shapes[shape_idx].mesh.indices.size();
			prim_mesh.vtx_count = (u32)shapes[shape_idx].mesh.num_face_vertices.size();
			prim_mesh.prim_idx = (u32)prim_meshes.size() - 1;

			auto entry = materials_to_objects.find(prim_mesh.filename);
			if (entry) {
				prim_mesh.material_idx = entry->value;
			} else if (material_entire_mesh_idx != U32_MAX) {
				prim_mesh.material_idx = material_entire_mesh_idx;
			} else {
				prim_mesh.material_idx = 0;	 // Default material
			}

			glm::vec3 min_vtx = glm::vec3(FLT_MAX);
			glm::vec3 max_vtx = glm::vec3(-FLT_MAX);
			u32 index_offset = 0;
			u32 idx_val = 0;
			MeshData per_mesh_data;
			for (u32 f = 0; f < shapes[shape_idx].mesh.num_face_vertices.size(); f++) {
				for (u32 v = 0; v < 3; v++) {
					tinyobj::index_t idx = shapes[shape_idx].mesh.indices[index_offset + v];
					per_mesh_data.indices.push_back(idx_val++);
					tinyobj::real_t vx = attrib.vertices[3 * u32(idx.vertex_index) + 0];
					tinyobj::real_t vy = attrib.vertices[3 * u32(idx.vertex_index) + 1];
					tinyobj::real_t vz = attrib.vertices[3 * u32(idx.vertex_index) + 2];
					per_mesh_data.positions.emplace_back(vx, vy, vz);
					min_vtx = glm::min(per_mesh_data.positions[per_mesh_data.positions.size() - 1], min_vtx);
					max_vtx = glm::max(per_mesh_data.positions[per_mesh_data.positions.size() - 1], max_vtx);
					if (idx.normal_index >= 0) {
						tinyobj::real_t nx = attrib.normals[3 * u32(idx.normal_index) + 0];
						tinyobj::real_t ny = attrib.normals[3 * u32(idx.normal_index) + 1];
						tinyobj::real_t nz = attrib.normals[3 * u32(idx.normal_index) + 2];
						per_mesh_data.normals.emplace_back(nx, ny, nz);
					}
					if (idx.texcoord_index >= 0) {
						tinyobj::real_t tx = attrib.texcoords[2 * u32(idx.texcoord_index) + 0];
						tinyobj::real_t ty = attrib.texcoords[2 * u32(idx.texcoord_index) + 1];
						per_mesh_data.texcoords0.emplace_back(tx, ty);
					}
				}
				index_offset += 3;
			}
			positions.insert(positions.end(), std::make_move_iterator(per_mesh_data.positions.begin()),
							 std::make_move_iterator(per_mesh_data.positions.end()));
			indices.insert(indices.end(), std::make_move_iterator(per_mesh_data.indices.begin()),
						   std::make_move_iterator(per_mesh_data.indices.end()));
			normals.insert(normals.end(), std::make_move_iterator(per_mesh_data.normals.begin()),
						   std::make_move_iterator(per_mesh_data.normals.end()));
			tangents.insert(tangents.end(), std::make_move_iterator(per_mesh_data.tangents.begin()),
							std::make_move_iterator(per_mesh_data.tangents.end()));
			texcoords0.insert(texcoords0.end(), std::make_move_iterator(per_mesh_data.texcoords0.begin()),
							  std::make_move_iterator(per_mesh_data.texcoords0.end()));
			texcoords1.insert(texcoords1.end(), std::make_move_iterator(per_mesh_data.texcoords1.begin()),
							  std::make_move_iterator(per_mesh_data.texcoords1.end()));
			colors0.insert(colors0.end(), std::make_move_iterator(per_mesh_data.colors0.begin()),
						   std::make_move_iterator(per_mesh_data.colors0.end()));

			prim_mesh.min_pos = min_vtx;
			prim_mesh.max_pos = max_vtx;
			prim_mesh.world_matrix = world_matrix;
		}
	}
	curr_config->cam_settings = CameraSettings{
		.fov = get_or_default_f(get_node(camera_node, "fov"), 90.0f),
		.pos = get_or_default_v3(get_node(camera_node, "position"), glm::vec3(-1.0f)),
		.rotation = get_or_default_v3(get_node(camera_node, "rotation"), glm::vec3(0.0f)),
		.dir = get_or_default_v3(get_node(camera_node, "dir"), glm::vec3(0.0f, 0.0f, -1.0f)),
	};
	compute_scene_dimensions();
}

void LumenScene::load_scene(const lm::String& path) {
	assert(path.is_cstr());
	os::FileHandle file_handle = os::file_open(path, os::AccessFlag_Read);
	if (file_handle == 0) {
		LUMEN_ERROR("Failed to open Lumen scene file: {}", path.data);
	}
	os::FileProperties props = os::file_properties(file_handle);
	lm::String file_content = lm::str_reserve(lm::arena(), props.size + 1);
	os::file_read(file_handle, file_content.data);
	file_content.data[props.size] = '\0';
	LumenNode* root = parse_scene(lm::arena(), file_content);
	if (!root) {
		LUMEN_ERROR("Failed to parse the file {}", path.data);
	}
	// TODO: Error check?
	lm::String path_root = lm::str_substr(path, 0, lm::str_rfind(path, "/\\") + 1);
	parse_lumen_scene(path, path_root, root);

	lm::camera_init(&camera, config->cam_settings.fov, 0.01f, 1000.0f, Window::aspect_ratio(), config->cam_settings.dir,
					config->cam_settings.pos, config->cam_settings.rotation);

	total_light_triangle_cnt = 0;
	total_light_area = 0;
	std::vector<PrimInfo> prim_lookup;
	u32 idx = 0;
	for (auto& pm : prim_meshes) {
		PrimInfo m_info;
		m_info.index_offset = pm.first_idx;
		m_info.vertex_offset = pm.vtx_offset;
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

	f32 total_light_triangle_area = 0.0f;
	for (auto& l : gpu_lights) {
		if ((l.light_flags & 0x7) == LIGHT_AREA) {
			const auto& pm = prim_meshes[l.prim_mesh_idx];
			l.world_matrix = pm.world_matrix;
			auto& idx_base_offset = pm.first_idx;
			auto& vtx_offset = pm.vtx_offset;
			for (u32 i = 0; i < l.num_triangles; i++) {
				auto idx_offset = idx_base_offset + 3 * i;
				glm::ivec3 ind = {indices[idx_offset], indices[idx_offset + 1], indices[idx_offset + 2]};
				ind += glm::vec3{vtx_offset, vtx_offset, vtx_offset};
				const vec3 v0 = pm.world_matrix * glm::vec4(positions[ind.x], 1.0);
				const vec3 v1 = pm.world_matrix * glm::vec4(positions[ind.y], 1.0);
				const vec3 v2 = pm.world_matrix * glm::vec4(positions[ind.z], 1.0);
				f32 area = 0.5f * glm::length(glm::cross(v1 - v0, v2 - v0));
				total_light_triangle_area += area;
			}
		}
	}
	mesh_lights_buffer = prm::get_buffer({.name = "Mesh Lights Buffer",
										  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
										  .memory_type = vk::BUFFER_TYPE_GPU,
										  .size = std::max(gpu_lights.size(), size_t(1)) * sizeof(Light),
										  .data = gpu_lights.data()});
	total_light_area += total_light_triangle_area;
	vertex_buffer = prm::get_buffer({.name = "Vertex Buffer",
									 .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
											  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
									 .memory_type = vk::BUFFER_TYPE_GPU,
									 .size = positions.size() * sizeof(glm::vec3),
									 .data = positions.data()});

	index_buffer = prm::get_buffer({.name = "Index Buffer",
									.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
											 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
									.memory_type = vk::BUFFER_TYPE_GPU,
									.size = indices.size() * sizeof(u32),
									.data = indices.data()});

	materials_buffer =
		prm::get_buffer({.name = "Materials Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = materials.size() * sizeof(Material),
						 .data = materials.data()});

	prim_lookup_buffer =
		prm::get_buffer({.name = "Prim Lookup Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = prim_lookup.size() * sizeof(PrimInfo),
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
						 .memory_type = vk::BUFFER_TYPE_GPU,
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
		i32 i = 0;
		for (const auto& texture_path : textures) {
			i32 x, y, n;
			lm::ScratchArena scratch = lm::arena();
			lm::String path  = lm::str_concat(scratch.arena, path_root, texture_path.relative_path, /*cstr=*/true);
			unsigned char* data = stbi_load(path.data, &x, &y, &n, 4);
			scene_textures[i] = prm::get_texture({.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												  .dimensions = {(u32)x, (u32)y, 1},
												  .format = VK_FORMAT_R8G8B8A8_SRGB,
												  .data = {.data = data, .size = u64(x * y * 4)},
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

static void add_leaf_node(lm::Arena* arena, LumenNode* node, const lm::String& name, const lm::String& value) {
	LUMEN_ASSERT(node, "Node cannot be null when adding field");
	LumenNode* prev_node = node->child;
	while (prev_node && prev_node->next) {
		prev_node = prev_node->next;
	}
	LumenNode* insertion_node = (LumenNode*)arena->allocate(sizeof(LumenNode));
	insertion_node->key = name;
	insertion_node->value = value;
	if (!prev_node) {
		node->child = insertion_node;
	} else {
		prev_node->next = insertion_node;
	}
	insertion_node->parent = node;
}

static void add_child_node(LumenNode* parent, LumenNode* child) {
	LUMEN_ASSERT(parent && child, "Parent and child nodes cannot be null when adding child node");
	if (!parent->child) {
		parent->child = child;
	} else {
		LumenNode* last_child = parent->child;
		while (last_child->next) {
			last_child = last_child->next;
		}
		last_child->next = child;
	}
	child->parent = parent;
}

static void add_list_node(lm::Arena* arena, LumenNode* parent, LumenNode* list_node) {
	LUMEN_ASSERT(parent && list_node, "Parent and list node cannot be null when adding list node");
	if (!parent->child) {
		parent->child = list_node;
	} else {
		LumenNode* last_child = parent->child;
		while (last_child->next) {
			last_child = last_child->next;
		}
		last_child->next = (LumenNode*)arena->allocate(sizeof(LumenNode));
		last_child->next->key = "-";
		last_child->next->child = list_node;
	}
}

static std::string to_lower(const std::string& str) {
	std::string lower_str = str;
	for (char& c : lower_str) {
		c = std::tolower(c);
	}
	return lower_str;
}

static lm::String str_from_vec3(lm::Arena* arena, const glm::vec3& vec) {
	lm::String result = "v3f(";
	result = lm::str_concat(arena, result, lm::str_from_f32(arena, vec.x));
	result = lm::str_concat(arena, result, ",");
	result = lm::str_concat(arena, result, lm::str_from_f32(arena, vec.y));
	result = lm::str_concat(arena, result, ",");
	result = lm::str_concat(arena, result, lm::str_from_f32(arena, vec.z));
	result = lm::str_concat(arena, result, ")");
	return result;
}

static lm::String get_material_type(const Material& mat) {
	switch (mat.bsdf_type) {
		case BSDF_TYPE_DIFFUSE:
			return "diffuse";
		case BSDF_TYPE_MIRROR:
			return "mirror";
		case BSDF_TYPE_GLASS:
			return "glass";
		case BSDF_TYPE_DIELECTRIC:
			return "dielectric";
		case BSDF_TYPE_CONDUCTOR:
			return "conductor";
		case BSDF_TYPE_PRINCIPLED:
			return "principled";
		default:
			LUMEN_ERROR("Unknown material type: {}", mat.bsdf_type);
			return "unknown";
	}
}

static void traverse_and_write_scene(lm::Arena* arena, lm::String& buffer, LumenNode* curr_node, i32 depth,
									 bool first_list_item = false) {
	if (!curr_node) {
		return;
	}
	if (!first_list_item) {
		for (i32 i = 0; i < depth; i++) {
			buffer = lm::str_concat(arena, buffer, " ");
		}
	}
	if (!curr_node->key.empty()) {
		buffer = lm::str_concat(arena, buffer, curr_node->key);
		if (curr_node->key == "-") {
			first_list_item = true;
			buffer = lm::str_concat(arena, buffer, " ");
		} else {
			buffer = lm::str_concat(arena, buffer, " ");
			buffer = lm::str_concat(arena, buffer, curr_node->value);
			buffer = lm::str_concat(arena, buffer, "\n");
		}
	}
	traverse_and_write_scene(arena, buffer, curr_node->child, depth + 1, first_list_item);
	if (curr_node->parent->key == "scene" || (!curr_node->next && curr_node->parent->key == "-")) {
		buffer = lm::str_concat(arena, buffer, "\n");
	}
	traverse_and_write_scene(arena, buffer, curr_node->next, depth);
}

void LumenScene::write_lumen_scene() {
	std::ofstream out_file("scene.scene");
	lm::ScratchArena scratch = lm::arena();

	LumenNode* root = new LumenNode();
	root->key = "scene";

	// Integrator settings
	LumenNode* integrator_node = new LumenNode{.key = "integrator"};
	lm::String integrator_name = lm::str_to_lower(scratch.arena, config->integrator_name);
	add_leaf_node(scratch.arena, integrator_node, "type", integrator_name);
	lm::String path_length = lm::str_from_u32(scratch.arena, config->path_length);
	add_leaf_node(scratch.arena, integrator_node, "path_length", path_length);
	lm::String sky_col = str_from_vec3(scratch.arena, config->sky_col);
	add_leaf_node(scratch.arena, integrator_node, "sky_col", sky_col);
	if (integrator_name == "sppm") {
		lm::String base_radius = lm::str_from_f32(scratch.arena, static_cast<SPPMConfig*>(config.get())->base_radius);
		add_leaf_node(scratch.arena, integrator_node, "base_radius", base_radius);
	} else if (integrator_name == "vcm") {
		lm::String enable_vm =
			lm::str_from_u32(scratch.arena, static_cast<VCMConfig*>(config.get())->enable_vm ? 1 : 0);
		lm::String radius_factor =
			lm::str_from_f32(scratch.arena, static_cast<VCMConfig*>(config.get())->radius_factor);
		add_leaf_node(scratch.arena, integrator_node, "enable_vm", enable_vm);
		add_leaf_node(scratch.arena, integrator_node, "radius_factor", radius_factor);
	} else if (integrator_name == "pssmlt") {
		lm::String mutations_per_pixel =
			lm::str_from_f32(scratch.arena, static_cast<PSSMLTConfig*>(config.get())->mutations_per_pixel);
		lm::String num_mlt_threads =
			lm::str_from_u32(scratch.arena, static_cast<PSSMLTConfig*>(config.get())->num_mlt_threads);
		lm::String num_bootstrap_samples =
			lm::str_from_u32(scratch.arena, static_cast<PSSMLTConfig*>(config.get())->num_bootstrap_samples);
		add_leaf_node(scratch.arena, integrator_node, "mutations_per_pixel", mutations_per_pixel);
		add_leaf_node(scratch.arena, integrator_node, "num_mlt_threads", num_mlt_threads);
		add_leaf_node(scratch.arena, integrator_node, "num_bootstrap_samples", num_bootstrap_samples);
	} else if (integrator_name == "smlt") {
		lm::String mutations_per_pixel =
			lm::str_from_f32(scratch.arena, static_cast<SMLTConfig*>(config.get())->mutations_per_pixel);
		lm::String num_mlt_threads =
			lm::str_from_u32(scratch.arena, static_cast<SMLTConfig*>(config.get())->num_mlt_threads);
		lm::String num_bootstrap_samples =
			lm::str_from_u32(scratch.arena, static_cast<SMLTConfig*>(config.get())->num_bootstrap_samples);
		add_leaf_node(scratch.arena, integrator_node, "mutations_per_pixel", mutations_per_pixel);
		add_leaf_node(scratch.arena, integrator_node, "num_mlt_threads", num_mlt_threads);
		add_leaf_node(scratch.arena, integrator_node, "num_bootstrap_samples", num_bootstrap_samples);
	} else if (integrator_name == "vcmmlt") {
		lm::String mutations_per_pixel =
			lm::str_from_f32(scratch.arena, static_cast<VCMMLTConfig*>(config.get())->mutations_per_pixel);
		lm::String num_mlt_threads =
			lm::str_from_u32(scratch.arena, static_cast<VCMMLTConfig*>(config.get())->num_mlt_threads);
		lm::String num_bootstrap_samples =
			lm::str_from_u32(scratch.arena, static_cast<VCMMLTConfig*>(config.get())->num_bootstrap_samples);
		lm::String radius_factor =
			lm::str_from_f32(scratch.arena, static_cast<VCMMLTConfig*>(config.get())->radius_factor);
		lm::String enable_vm =
			lm::str_from_u32(scratch.arena, static_cast<VCMMLTConfig*>(config.get())->enable_vm ? 1 : 0);
		lm::String alternate =
			lm::str_from_u32(scratch.arena, static_cast<VCMMLTConfig*>(config.get())->alternate ? 1 : 0);
		lm::String light_first =
			lm::str_from_u32(scratch.arena, static_cast<VCMMLTConfig*>(config.get())->light_first ? 1 : 0);
		add_leaf_node(scratch.arena, integrator_node, "mutations_per_pixel", mutations_per_pixel);
		add_leaf_node(scratch.arena, integrator_node, "num_mlt_threads", num_mlt_threads);
		add_leaf_node(scratch.arena, integrator_node, "num_bootstrap_samples", num_bootstrap_samples);
		add_leaf_node(scratch.arena, integrator_node, "radius_factor", radius_factor);
		add_leaf_node(scratch.arena, integrator_node, "enable_vm", enable_vm);
		add_leaf_node(scratch.arena, integrator_node, "alternate", alternate);
		add_leaf_node(scratch.arena, integrator_node, "light_first", light_first);
	}
	add_child_node(root, integrator_node);
	// Scene textures
	LumenNode* textures_node = new LumenNode{.key = "textures"};
	for (const TextureRef& texture_ref : textures) {
		const lm::String& texture_name = texture_ref.name;
		LumenNode* texture_prop = new LumenNode{.key = "-"};
		add_leaf_node(scratch.arena, texture_prop, "name", texture_name);
		add_leaf_node(scratch.arena, texture_prop, "file", texture_ref.relative_path);
		add_child_node(textures_node, texture_prop);
	}
	add_child_node(root, textures_node);
	// Scene materials
	LumenNode* bsdfs_node = new LumenNode{.key = "bsdfs"};
	for (u32 i = 0; i < materials.size(); i++) {
		LumenNode* bsdf_prop = new LumenNode{.key = "-"};
		const Material& mat = materials[i];
		auto material_entry = material_idx_to_name.find(i);
		assert(material_entry);
		add_leaf_node(scratch.arena, bsdf_prop, "name", material_entry->value);
		add_leaf_node(scratch.arena, bsdf_prop, "type", get_material_type(mat));
		lm::String albedo = str_from_vec3(scratch.arena, mat.albedo);
		add_leaf_node(scratch.arena, bsdf_prop, "albedo", albedo);
		if (glm::any(glm::greaterThan(mat.emissive_factor, glm::vec3(0.0f)))) {
			lm::String emissive_factor = str_from_vec3(scratch.arena, mat.emissive_factor);
			add_leaf_node(scratch.arena, bsdf_prop, "emissive_factor", emissive_factor);
		}
		if (mat.texture_id != -1) {
			add_leaf_node(scratch.arena, bsdf_prop, "texture", textures[mat.texture_id].name);
		}
		switch (mat.bsdf_type) {
			case BSDF_TYPE_GLASS: {
				lm::String ior = lm::str_from_f32(scratch.arena, mat.ior);
				add_leaf_node(scratch.arena, bsdf_prop, "ior", ior);
			} break;
			case BSDF_TYPE_DIELECTRIC: {
				lm::String ior = lm::str_from_f32(scratch.arena, mat.ior);
				add_leaf_node(scratch.arena, bsdf_prop, "ior", ior);
				lm::String roughness = lm::str_from_f32(scratch.arena, mat.roughness);
				add_leaf_node(scratch.arena, bsdf_prop, "roughness", roughness);
				if (mat.bsdf_props & BSDF_FLAG_TRANSMISSION) {
					add_leaf_node(scratch.arena, bsdf_prop, "transmission", "1");
				}
				if (mat.bsdf_props & BSDF_FLAG_REFLECTION) {
					add_leaf_node(scratch.arena, bsdf_prop, "reflection", "1");
				}
				if (mat.thin) {
					add_leaf_node(scratch.arena, bsdf_prop, "thin", "1");
				}

			} break;
			case BSDF_TYPE_CONDUCTOR: {
				add_leaf_node(scratch.arena, bsdf_prop, "roughness", lm::str_from_f32(scratch.arena, mat.roughness));

				// https://jcgt.org/published/0003/04/03/paper.pdf , Eqn. 14 and 15
				// Note: albedo = eta = n from the paper
				glm::vec3 r_num = mat.albedo - glm::vec3(1.0f);
				glm::vec3 r_denom = mat.albedo + glm::vec3(1.0f);
				glm::vec3 reflectivity = (r_num * r_num + mat.k * mat.k) / (r_denom * r_denom + mat.k * mat.k);
				glm::vec3 n_min = (1.0f - reflectivity) / (1.0f + reflectivity);
				glm::vec3 n_max = (1.0f + glm::sqrt(reflectivity)) / (1.0f - glm::sqrt(reflectivity));

				glm::vec3 edge_tint;
				for (i32 c = 0; c < 3; c++) {
					if (reflectivity[c] == 0)
						edge_tint[c] = 0.0f;
					else if (reflectivity[c] == 1.0)
						edge_tint[c] = 1.0f;
					else
						edge_tint[c] = (n_max[c] - mat.albedo[c]) / (n_max[c] - n_min[c]);
				}
				add_leaf_node(scratch.arena, bsdf_prop, "reflectivity", str_from_vec3(scratch.arena, reflectivity));
				add_leaf_node(scratch.arena, bsdf_prop, "edge_tint", str_from_vec3(scratch.arena, edge_tint));

			} break;
			case BSDF_TYPE_PRINCIPLED: {
				add_leaf_node(scratch.arena, bsdf_prop, "ior", lm::str_from_f32(scratch.arena, mat.ior));
				add_leaf_node(scratch.arena, bsdf_prop, "roughness", lm::str_from_f32(scratch.arena, mat.roughness));
				add_leaf_node(scratch.arena, bsdf_prop, "diffuse_transmission",
							  lm::str_from_f32(scratch.arena, mat.diffuse_trans));
				add_leaf_node(scratch.arena, bsdf_prop, "specular_transmission",
							  lm::str_from_f32(scratch.arena, mat.spec_trans));
				add_leaf_node(scratch.arena, bsdf_prop, "metallic", lm::str_from_f32(scratch.arena, mat.metallic));
				add_leaf_node(scratch.arena, bsdf_prop, "specular_tint",
							  lm::str_from_f32(scratch.arena, mat.specular_tint));
				add_leaf_node(scratch.arena, bsdf_prop, "sheen_tint", lm::str_from_f32(scratch.arena, mat.sheen_tint));
				add_leaf_node(scratch.arena, bsdf_prop, "clearcoat", lm::str_from_f32(scratch.arena, mat.clearcoat));
				add_leaf_node(scratch.arena, bsdf_prop, "clearcoat_gloss",
							  lm::str_from_f32(scratch.arena, mat.clearcoat_gloss));
				add_leaf_node(scratch.arena, bsdf_prop, "subsurface", lm::str_from_f32(scratch.arena, mat.subsurface));
				add_leaf_node(scratch.arena, bsdf_prop, "flatness", lm::str_from_f32(scratch.arena, mat.flatness));
				add_leaf_node(scratch.arena, bsdf_prop, "sheen", lm::str_from_f32(scratch.arena, mat.sheen));
				add_leaf_node(scratch.arena, bsdf_prop, "anisotropy", lm::str_from_f32(scratch.arena, mat.anisotropy));
				add_leaf_node(scratch.arena, bsdf_prop, "thin", lm::str_from_u32(scratch.arena, mat.thin));

			} break;
		}
		add_child_node(bsdfs_node, bsdf_prop);
	}
	add_child_node(root, bsdfs_node);

	// Analytical lights
	if (!lights.empty()) {
		LumenNode* lights_node = new LumenNode{.key = "lights"};
		for (const LumenLight& light : lights) {
			LumenNode* light_node = new LumenNode{.key = "-"};

			add_leaf_node(scratch.arena, light_node, "type", get_light_type_str(light));
			add_leaf_node(scratch.arena, light_node, "L", str_from_vec3(scratch.arena, light.L));
			add_leaf_node(scratch.arena, light_node, "pos", str_from_vec3(scratch.arena, light.pos));
			add_leaf_node(scratch.arena, light_node, "dir", str_from_vec3(scratch.arena, light.to));
			add_child_node(lights_node, light_node);
		}
		add_child_node(root, lights_node);
	}

	// Camera settings
	LumenNode* camera_node = new LumenNode{.key = "camera"};
	add_leaf_node(scratch.arena, camera_node, "fov", lm::str_from_f32(scratch.arena, config->cam_settings.fov));
	add_leaf_node(scratch.arena, camera_node, "position", str_from_vec3(scratch.arena, camera.position));
	add_leaf_node(scratch.arena, camera_node, "rotation", str_from_vec3(scratch.arena, camera.rotation));
	add_leaf_node(scratch.arena, camera_node, "dir", str_from_vec3(scratch.arena, camera.direction));
	add_child_node(root, camera_node);

	// Mesh and material mappings
	if (!prim_meshes.empty()) {
		// File name to mesh/sub-mesh mapping
		auto mesh_mappings = lm::hash_map_create<lm::String, lm::Array<LumenPrimMesh*>>(scratch.arena);

		for (LumenPrimMesh& mesh : prim_meshes) {
			auto entry = mesh_mappings.get_or_create(mesh.filename);
			if(!entry->value.initialized()) {
				entry->value = lm::array_create<LumenPrimMesh*>(scratch.arena);
			}
			entry->value.push_back(&mesh);
		}

		LumenNode* meshes_node = new LumenNode{.key = "mesh"};
		for (const auto& mesh_mapping : mesh_mappings) {

			LumenNode* mesh_node = new LumenNode{.key = "-"};
			add_leaf_node(scratch.arena, mesh_node, "name", mesh_mapping.key);
			LumenNode* materials_node = new LumenNode{.key = "materials"};
			LumenNode* transforms_node = new LumenNode{.key = "transforms"};
			add_child_node(mesh_node, materials_node);
			bool has_transform = false;
			for (LumenPrimMesh* mesh : mesh_mapping.value) {
				LumenNode* mesh_to_material_node = new LumenNode{.key = "-"};
				if (!mesh->name.empty()) {
					add_leaf_node(scratch.arena, mesh_to_material_node, "refs", mesh->name);
				}

				auto material_entry = material_idx_to_name.find(mesh->material_idx);
				assert(material_entry);
				add_leaf_node(scratch.arena, mesh_to_material_node, "name", material_entry->value);
				add_child_node(materials_node, mesh_to_material_node);

				glm::vec3 scale;
				glm::quat q;
				glm::vec3 translation;
				glm::vec3 skew;
				glm::vec4 perspective;
				glm::decompose(mesh->world_matrix, scale, q, translation, skew, perspective);
				glm::vec3 rot{};
				glm::extractEulerAngleXYZ(glm::toMat4(q), rot.x, rot.y, rot.z);
				if (translation != glm::vec3(0.0)) {
					add_leaf_node(scratch.arena, transforms_node, "translation",
								  str_from_vec3(scratch.arena, translation));
					has_transform = true;
				}
				if (rot != glm::vec3(0.0)) {
					add_leaf_node(scratch.arena, transforms_node, "rotation",
								  str_from_vec3(scratch.arena, glm::degrees(rot)));
					has_transform = true;
				}
				if (scale != glm::vec3(1.0)) {
					add_leaf_node(scratch.arena, transforms_node, "scale", str_from_vec3(scratch.arena, scale));
					has_transform = true;
				}
			}
			if (has_transform) {
				add_child_node(mesh_node, transforms_node);
			}

			add_child_node(meshes_node, mesh_node);
		}
		add_child_node(root, meshes_node);
	}

	// Write the scene
	os::FileHandle handle = os::file_open("scene.scene", os::AccessFlag_Write);
	lm::String buffer;

	traverse_and_write_scene(scratch.arena, buffer, root->child, 0);
	os::file_write(handle, (void*)buffer.data, buffer.size);
	os::file_close(handle);

	// __debugbreak();
}

void LumenScene::add_default_texture() {
	std::array<u8, 4> nil = {0, 0, 0, 0};
	scene_textures.resize(1);
	scene_textures[0] = prm::get_texture({.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
										  .dimensions = {1, 1, 1},
										  .format = VK_FORMAT_R8G8B8A8_SRGB,
										  .data = {.data = nil.data(), .size = 4},
										  .sampler = texture_sampler});
}

void LumenScene::create_scene_config(const lm::String& integrator_name) {
	lm::ScratchArena scratch = lm::arena();
	lm::String lowercase_name = lm::str_to_lower(scratch.arena, integrator_name);
	if (lowercase_name == "path") {
		config = std::make_unique<PathConfig>();
	} else if (lowercase_name == "bdpt") {
		config = std::make_unique<BDPTConfig>();
	} else if (lowercase_name == "sppm") {
		config = std::make_unique<SPPMConfig>();
	} else if (lowercase_name == "vcm") {
		config = std::make_unique<VCMConfig>();
	} else if (lowercase_name == "pssmlt") {
		config = std::make_unique<PSSMLTConfig>();
	} else if (lowercase_name == "smlt") {
		config = std::make_unique<SMLTConfig>();
	} else if (lowercase_name == "vcmmlt") {
		config = std::make_unique<VCMMLTConfig>();
	} else if (lowercase_name == "restir") {
		config = std::make_unique<ReSTIRConfig>();
	} else if (lowercase_name == "restirgi") {
		config = std::make_unique<ReSTIRGIConfig>();
	} else if (lowercase_name == "restirpt") {
		config = std::make_unique<ReSTIRPTConfig>();
	} else if (lowercase_name == "ddgi") {
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
	std::vector<vk::Buffer*> buffer_list = {index_buffer,	  vertex_buffer,	  compact_vertices_buffer,
											materials_buffer, prim_lookup_buffer, mesh_lights_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
	for (vk::Texture* tex : scene_textures) {
		prm::remove(tex);
	}
	vkDestroySampler(vk::context().device, texture_sampler, nullptr);
}