#include "LumenScene.h"
#include "Framework/Base/BBox.h"
#include "Framework/Base/HashMap.h"
#include "Framework/Base/OS.h"
#include "Framework/PersistentResourceManager.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"
#include <fast_obj.h>
#include <stb/stb_image.h>

// TODO: Add instancing to the scene format
namespace scene {

static lm::Arena* _arena_scene = nullptr;
static lm::Arena* _arena_strings = nullptr;
static Scene _scene = {};

static void reflectance_to_conductor_eta_k(const lm::vec3& reflectance, lm::vec3& eta, lm::vec3& k) {
	eta = lm::vec3(1.0f);
	k = 2.0f * lm::sqrt(reflectance) / lm::sqrt(lm::max(lm::vec3(1.0f) - reflectance, 0.001f));
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

static LumenNode* node_create(lm::Arena* arena, lm::String key, lm::String val = {}) {
	LumenNode* node =
		(LumenNode*)arena->allocate(sizeof(LumenNode), alignof(LumenNode), nullptr, /*zero_initialize=*/true);
	node->key = key;
	node->value = val;
	return node;
}

static LumenNode* file_parse(lm::String buffer) {
	buffer[buffer.size - 1] = '\n';
	LumenNode* root = node_create(_arena_strings, "root");
	LumenNode* stack[64];
	i32 stack_size = 0;

	u64 curr_line_idx = 0;
	i32 base_indentation = 0;
	for (u64 buffer_cursor = 0; buffer_cursor < buffer.size; buffer_cursor++) {
		// Note: Currently only braces are allowed for this
		if (buffer[buffer_cursor] == '[') {
		}
		u64 line_end_idx = U64_MAX;
#if defined(_WIN32) || defined(_WIN64)
		// Handle newline
		if (buffer[buffer_cursor] == '\r') {
			line_end_idx = buffer_cursor++;
		}
#endif
		if (buffer[buffer_cursor] == '\n') {
			line_end_idx = lm::min(line_end_idx, buffer_cursor);
			assert(line_end_idx != U64_MAX);
			u64 line_cursor = curr_line_idx;
			i32 indentation = 0;
			bool list_item = false;
			while (lm::char_is_whitespace(buffer[line_cursor]) || buffer[line_cursor] == '-') {
				if (buffer[line_cursor] == '-') {
					list_item = true;
				}
				line_cursor++;
				indentation++;
			}
			// Empty new line
			if (line_cursor == line_end_idx) {
				curr_line_idx = buffer_cursor + 1;
				continue;
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
				LumenNode* temp = node_create(_arena_strings, "-");
				stack[stack_size++] = temp;
				LUMEN_ASSERT(stack_size >= 2, "LumenNode list item level must be at least 2");
				temp->parent = stack[stack_size - 2];
				insert_child(stack[stack_size - 2], temp);
			}

			u64 colon_idx = line_cursor;
			while (buffer[colon_idx] != ':') colon_idx++;
			if (colon_idx >= line_end_idx) {
				LUMEN_ERROR("Malformed scene file, missing ':'");
			}
			LumenNode* node = node_create(_arena_strings, lm::str_substr(buffer, line_cursor, colon_idx - line_cursor));

			LumenNode* prev = level > 0 ? stack[stack_size - 1] : root;
			node->parent = prev;
			line_cursor = colon_idx + 1;
			while (lm::char_is_whitespace(buffer[line_cursor])) line_cursor++;
			if (line_cursor < line_end_idx) {
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
			}

			if (prev) {
				insert_child(prev, node);
			}
			LUMEN_ASSERT(stack_size < 64, "LumenNode stack overflow");
			stack[stack_size++] = node;
			curr_line_idx = buffer_cursor + 1;
		}
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

static lm::String get_str(LumenNode* node) { return node->value; }
static lm::String get_or_default_str(LumenNode* node, const lm::String& val) {
	if (!node || node->value.empty()) {
		return val;
	}
	return node->value;
}

static void get_or_default_i_2(LumenNode* node, i32& result) {
	if (!node || node->value.empty()) {
		return;
	}
	result = lm::i32_from_str(node->value);
}

static void get_or_default_u_2(LumenNode* node, u32& result) {
	if (!node || node->value.empty()) {
		return;
	}
	result = lm::u32_from_str(node->value);
}

static i32 get_or_default_i(LumenNode* node, i32 val) {
	i32 result = val;
	get_or_default_i_2(node, result);
	return result;
}

static void get_or_default_bool_2(LumenNode* node, bool& result) {
	if (!node || node->value.empty()) {
		return;
	}
	result = lm::i32_from_str(node->value) == 1;
}

static void get_or_default_f_2(LumenNode* node, f32& result) {
	if (!node || node->value.empty()) {
		return;
	}
	result = lm::f32_from_str(node->value);
}

static f32 get_or_default_f(LumenNode* node, f32 val) {
	f32 result = val;
	get_or_default_f_2(node, result);
	return result;
}

static lm::FixedArray<lm::String> get_str_list(lm::Arena* arena, LumenNode* node) {
	if (!node || node->value.empty()) {
		return {};
	}
	auto result = lm::fixed_array_create<lm::String>(arena, node->num_list_items);
	u64 i = 0;
	while (i < node->value.size) {
		while (i < node->value.size && lm::char_is_whitespace(node->value[i])) i++;
		u64 start = i;
		while (i < node->value.size && node->value[i] != ',') i++;
		u64 end = i - 1;
		while (lm::char_is_whitespace(node->value[end])) end--;
		result.emplace_back(&node->value[start], end - start + 1);
		i++;
	}
	return result;
}

static lm::String get_light_type_str(const AnalyticalLight& light) {
	if (light.light_flags & LIGHT_SPOT) {
		return "spot";
	} else if (light.light_flags & LIGHT_POINT) {
		return "point";
	} else if (light.light_flags & LIGHT_DIRECTIONAL) {
		return "directional";
	}
	return "";
}

static void get_or_default_v3_2(LumenNode* node, lm::vec3& result) {
	if (!node || node->value.empty()) {
		return;
	}
	char* ptr = node->value.data;
	while (*ptr != '(') {
		ptr++;
	}
	i32 comma_count = 0;
	char* start = ++ptr;
	while (*ptr != ')') {
		if (*ptr == ',') {
			lm::String val = lm::String(start, ptr - start);
			result[comma_count++] = lm::f32_from_str(val);
			start = ptr + 1;
		}
		ptr++;
	}
	LUMEN_ASSERT(comma_count == 2, "Expected 3 items in vec3");
	result[comma_count++] = lm::f32_from_str(lm::String(start, ptr - start));
}

static lm::vec3 get_or_default_v3(LumenNode* node, const lm::vec3& val) {
	lm::vec3 result = val;
	get_or_default_v3_2(node, result);
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

static lm::String str_from_vec3(lm::Arena* arena, const lm::vec3& vec) {
	char buf[64];
	i32 num_chars = stbsp_snprintf(buf, sizeof(buf), "v3f(%.6g,%.6g,%.6g)", vec.x, vec.y, vec.z);
	lm::String result;
	result.size = num_chars;
	result.data = (char*)arena->allocate(result.size);
	memmove(result.data, buf, sizeof(buf));
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
			LUMEN_ERROR("Unknown material type: %d", mat.bsdf_type);
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

static void scene_init(const lm::String& path_root, LumenNode* root) {
	LumenNode* integrator_node = get_node(root, "integrator");
	LumenNode* bsdfs_node = get_node(root, "bsdfs");
	LumenNode* camera_node = get_node(root, "camera");
	LumenNode* lights_node = get_node(root, "lights");
	LumenNode* textures_node = get_node(root, "textures");
	LumenNode* meshes_node = get_node(root, "mesh");

	////////////////////////////
	// --- Scene memory allocation ---

	auto mesh_to_obj_map = lm::hash_map_create<u32, fastObjMesh*>(_arena_scene);
	u64 total_obj_count = 0;
	u64 total_vtx_count = 0;
	u32 mesh_idx = 0;
	for (LumenNode* mesh_node = meshes_node->child; mesh_node; mesh_node = next_node(mesh_node), mesh_idx++) {
		lm::ScratchArena scratch = _arena_strings;
		lm::String relative_mesh_file =
			lm::str_to_cstr(scratch.arena, get_or_default_str(get_node(mesh_node, "name"), CSTR("")));
		lm::String mesh_file = lm::str_concat(scratch.arena, path_root, relative_mesh_file);
		fastObjMesh* obj = fast_obj_read(mesh_file.data);
		mesh_to_obj_map.insert(mesh_idx, obj);
		total_obj_count += obj->object_count;
		for (u32 i = 0; i < obj->face_count; i++) {
			total_vtx_count += 3 * obj->face_vertices[i] - 6;
		}
	}

	// GPU Lights are allocated later
	_scene.positions = lm::fixed_array_create<lm::vec3>(_arena_scene, total_vtx_count);
	_scene.indices = lm::fixed_array_create<u32>(_arena_scene, total_vtx_count);
	_scene.normals = lm::fixed_array_create<lm::vec3>(_arena_scene, total_vtx_count);
	_scene.texcoords0 = lm::fixed_array_create<lm::vec2>(_arena_scene, total_vtx_count);
	_scene.prim_meshes = lm::fixed_array_create<LumenPrimMesh>(_arena_scene, total_obj_count);
	_scene.materials = lm::fixed_array_create<Material>(_arena_scene, get_child_count(bsdfs_node));
	_scene.textures = lm::fixed_array_create<TextureRef>(_arena_scene, get_child_count(textures_node));
	_scene.scene_textures = lm::fixed_array_create<vk::Texture*>(_arena_scene, lm::max(_scene.textures.capacity, 1ull));
	_scene.analytical_lights = lm::fixed_array_create<AnalyticalLight>(_arena_scene, get_child_count(lights_node));
	_scene.material_idx_to_name = lm::hash_map_create<u32, lm::String>(_arena_scene, get_child_count(bsdfs_node));

	////////////////////////////
	// --- Scene initialization ---

	lm::String integrator_type = get_or_default_str(get_node(integrator_node, "type"), "path");
	SceneCommon common_config = {};
	get_or_default_u_2(get_node(integrator_node, "path_length"), common_config.path_length);
	get_or_default_v3_2(get_node(integrator_node, "sky_col"), common_config.sky_col);
	get_or_default_f_2(get_node(camera_node, "fov"), common_config.cam_settings.fov);
	get_or_default_v3_2(get_node(camera_node, "position"), common_config.cam_settings.pos);
	get_or_default_v3_2(get_node(camera_node, "rotation"), common_config.cam_settings.rotation);
	get_or_default_v3_2(get_node(camera_node, "dir"), common_config.cam_settings.dir);
	config_init(integrator_type, common_config, integrator_node);

	lm::ScratchArena scratch = _arena_scene;
	lm::HashMap<lm::String, u32> material_map = lm::hash_map_create<lm::String, u32>(scratch.arena);
	lm::HashMap<lm::String, u32> materials_to_objects = lm::hash_map_create<lm::String, u32>(scratch.arena);
	lm::HashMap<lm::String, u32> texture_name_to_idx = lm::hash_map_create<lm::String, u32>(scratch.arena);

	// Textures
	if (textures_node) {
		for (LumenNode* texture_node = textures_node->child; texture_node; texture_node = next_node(texture_node)) {
			lm::String name = get_str(get_node(texture_node, "name"));
			lm::String file = get_str(get_node(texture_node, "file"));
			LUMEN_ASSERT(!name.empty() && !file.empty(), "Texture name and file must be specified");
			TextureRef ref;
			ref.name = name;
			ref.allocation_name = lm::str_to_cstr(_arena_strings, name);
			ref.relative_path = lm::str_to_cstr(_arena_strings, file);
			_scene.textures.push_back(ref);
			texture_name_to_idx.insert(name, (u32)_scene.textures.size - 1);
		}
	}

	// Materials
	if (bsdfs_node) {
		u32 bsdf_idx = 0;
		for (LumenNode* bsdf_node = bsdfs_node->child; bsdf_node; bsdf_node = next_node(bsdf_node), bsdf_idx++) {
			Material& material = _scene.materials.emplace_back();
			material.albedo = get_or_default_v3(get_node(bsdf_node, "albedo"), lm::vec3(1.0f));
			material.emissive_factor = get_or_default_v3(get_node(bsdf_node, "emissive_factor"), lm::vec3(0.0f));
			material.emission_two_sided = get_or_default_i(get_node(bsdf_node, "two_sided"), 0);
			material.texture_id = -1;
			lm::String mat_name = get_str(get_node(bsdf_node, "name"));
			if (!mat_name.empty()) {
				material_map.insert(mat_name, bsdf_idx);
				_scene.material_idx_to_name.insert(bsdf_idx, mat_name);
			}

			lm::String texture_name = get_or_default_str(get_node(bsdf_node, "texture"), "");
			if (!texture_name.empty()) {
				auto* entry = texture_name_to_idx.find(texture_name);
				if (entry) {
					material.texture_id = entry->value;
				}
			}

			lm::String type = get_or_default_str(get_node(bsdf_node, "type"), "diffuse");
			if (type == "diffuse") {
				_scene.bsdf_types |= BSDF_TYPE_DIFFUSE;
				material.bsdf_type = BSDF_TYPE_DIFFUSE;
				material.bsdf_props = BSDF_FLAG_DIFFUSE_REFLECTION;
			} else if (type == "mirror") {
				_scene.bsdf_types |= BSDF_TYPE_MIRROR;
				material.bsdf_type = BSDF_TYPE_MIRROR;
				material.bsdf_props = BSDF_FLAG_SPECULAR_REFLECTION;
			} else if (type == "glass") {
				_scene.bsdf_types |= BSDF_TYPE_GLASS;
				material.bsdf_type = BSDF_TYPE_GLASS;
				material.bsdf_props = BSDF_FLAG_SPECULAR_TRANSMISSION;
				material.ior = get_or_default_f(get_node(bsdf_node, "ior"), 1.0f);
			} else if (type == "dielectric") {
				_scene.bsdf_types |= BSDF_TYPE_DIELECTRIC;
				material.bsdf_type = BSDF_TYPE_DIELECTRIC;

				material.ior = get_or_default_f(get_node(bsdf_node, "ior"), 1.0f);
				material.roughness = get_or_default_f(get_node(bsdf_node, "roughness"), 0.0f);

				LumenNode* transmission_node = get_node(bsdf_node, "transmission");
				LumenNode* reflection_node = get_node(bsdf_node, "reflection");
				bool transmission = !transmission_node || get_or_default_i(transmission_node, 1);
				bool reflection = !reflection_node || get_or_default_i(reflection_node, 1);
				if (transmission) {
					material.bsdf_props |= BSDF_FLAG_TRANSMISSION;
				}
				if (reflection) {
					material.bsdf_props |= BSDF_FLAG_REFLECTION;
				}
				if (material.ior != 1.0f && material.roughness > 0.08f) {
					material.bsdf_props |= BSDF_FLAG_GLOSSY;
				} else {
					material.bsdf_props |= BSDF_FLAG_SPECULAR;
				}
				material.thin = get_or_default_i(get_node(bsdf_node, "thin"), 0);
			} else if (type == "conductor") {
				_scene.bsdf_types |= BSDF_TYPE_CONDUCTOR;
				material.bsdf_type = BSDF_TYPE_CONDUCTOR;
				material.roughness = get_or_default_f(get_node(bsdf_node, "roughness"), 0.0f);

				// In conductor context, albedo is used as eta (i.e the IOR)
				// k is the absorption coefficient
				LumenNode* reflectance = get_node(bsdf_node, "reflectance");
				if (reflectance) {
					lm::vec3 reflectance_val = lm::clamp(get_or_default_v3(reflectance, lm::vec3(1.0f)), 0.0f, 0.9999f);
					reflectance_to_conductor_eta_k(reflectance_val, material.albedo, material.k);
				}

				// Apply the mappings from https://jcgt.org/published/0003/04/03/paper.pdf
				LumenNode* edge_tint = get_node(bsdf_node, "edge_tint");
				LumenNode* reflectivity = get_node(bsdf_node, "reflectivity");
				if (edge_tint && reflectivity) {
					// The mapping is singular at a reflectivity of 1, so we clamp
					lm::vec3 edge_tint_vec = lm::clamp(get_or_default_v3(edge_tint, lm::vec3(1)), 0.0f, 1.0f);
					lm::vec3 reflectivity_vec = lm::clamp(get_or_default_v3(reflectivity, lm::vec3(0)), 0.0f, 0.99f);
					material.albedo = edge_tint_vec * (1.0f - reflectivity_vec) / (1.0f + reflectivity_vec) +
									  (1.0f - edge_tint_vec) * (1.0f + lm::sqrt(reflectivity_vec)) /
										  (1.0f - lm::sqrt(reflectivity_vec));
					lm::vec3 intermediate_term = material.albedo + 1.0f;
					lm::vec3 intermediate_term2 = material.albedo - 1.0f;
					material.k = lm::sqrt(lm::max(1.0f / (1.0f - reflectivity_vec) *
													  (reflectivity_vec * intermediate_term * intermediate_term -
													   intermediate_term2 * intermediate_term2),
												  lm::vec3(0.0f)));
				}

				material.bsdf_props = BSDF_FLAG_REFLECTION;
				if (material.roughness > 0.08f) {
					material.bsdf_props |= BSDF_FLAG_GLOSSY;
				} else {
					material.bsdf_props |= BSDF_FLAG_SPECULAR;
				}
			} else if (type == "principled") {
				_scene.bsdf_types |= BSDF_TYPE_PRINCIPLED;
				material.bsdf_type = BSDF_TYPE_PRINCIPLED;
				material.albedo = get_or_default_v3(get_node(bsdf_node, "albedo"), lm::vec3(1));
				material.ior = get_or_default_f(get_node(bsdf_node, "ior"), 1.0f);
				material.roughness = get_or_default_f(get_node(bsdf_node, "roughness"), 0.5f);
				material.diffuse_trans = get_or_default_f(get_node(bsdf_node, "diffuse_transmission"), 0.0f);
				material.spec_trans = get_or_default_f(get_node(bsdf_node, "specular_transmission"), 0.0f);
				material.metallic = get_or_default_f(get_node(bsdf_node, "metallic"), 0.0f);
				material.specular_tint = get_or_default_f(get_node(bsdf_node, "specular_tint"), 0.0f);
				material.sheen_tint = get_or_default_f(get_node(bsdf_node, "sheen_tint"), 0.5f);
				material.clearcoat = get_or_default_f(get_node(bsdf_node, "clearcoat"), 0.0f);
				material.clearcoat_gloss = get_or_default_f(get_node(bsdf_node, "clearcoat_gloss"), 1.0f);
				material.subsurface = get_or_default_f(get_node(bsdf_node, "subsurface"), 0.0f);
				material.flatness = get_or_default_f(get_node(bsdf_node, "flatness"), 0.0f);
				material.sheen = get_or_default_f(get_node(bsdf_node, "sheen"), 0.0f);
				material.anisotropy = get_or_default_f(get_node(bsdf_node, "anisotropy"), 0.0f);
				material.thin = get_or_default_i(get_node(bsdf_node, "thin"), 0);

				if (material.diffuse_trans != 0.0f) {
					LUMEN_WARN("Principled material %u: diffuse_transmission is unsupported and will be ignored",
							   bsdf_idx);
					material.diffuse_trans = 0.0f;
				}
				if (material.sheen != 0.0f) {
					LUMEN_WARN("Principled material %u: sheen is unsupported and will be ignored", bsdf_idx);
					material.sheen = 0.0f;
				}
				if (material.subsurface != 0.0f) {
					LUMEN_WARN("Principled material %u: subsurface is unsupported and will be ignored", bsdf_idx);
					material.subsurface = 0.0f;
				}

				if (material.roughness < 1.0f) {
					material.bsdf_props |= BSDF_FLAG_REFLECTION;
				}
				if (material.spec_trans > 0.0f) {
					material.bsdf_props |= BSDF_FLAG_TRANSMISSION;
				}
				if (material.roughness > 0.08f) {
					material.bsdf_props |= BSDF_FLAG_GLOSSY;
				} else {
					material.bsdf_props |= BSDF_FLAG_SPECULAR;
				}
			}
		}
	}

	// Analytical Lights
	if (lights_node) {
		for (LumenNode* light_node = lights_node->child; light_node; light_node = next_node(light_node)) {
			lm::String type = get_str(get_node(light_node, "type"));
			AnalyticalLight& light = _scene.analytical_lights.emplace_back();
			light.L = get_or_default_v3(get_node(light_node, "L"), lm::vec3(0));
			light.pos = get_or_default_v3(get_node(light_node, "pos"), lm::vec3(0));
			light.to = get_or_default_v3(get_node(light_node, "dir"), lm::vec3(0, 0, 1));
			if (type == "point") {
				light.light_flags = LIGHT_POINT | LIGHT_FLAG_FINITE | LIGHT_FLAG_DELTA | LIGHT_FLAG_DELTA_POSITION;
			} else if (type == "spot") {
				light.light_flags = LIGHT_SPOT | LIGHT_FLAG_FINITE | LIGHT_FLAG_DELTA | LIGHT_FLAG_DELTA_POSITION;
				light.outer_angle = get_or_default_f(get_node(light_node, "outer_angle"), 30.0f);
				light.inner_angle = get_or_default_f(get_node(light_node, "inner_angle"), 25.0f);
				LUMEN_ASSERT(light.outer_angle > 0.0f && light.outer_angle <= 180.0f,
							 "Spot light outer_angle must be in (0, 180] degrees");
				LUMEN_ASSERT(light.inner_angle >= 0.0f, "Spot light inner_angle must be non-negative");
				if (light.inner_angle > light.outer_angle) {
					LUMEN_WARN("Spot light inner_angle must not exceed outer_angle; using %f degrees",
							   light.outer_angle);
					light.inner_angle = light.outer_angle;
				}
			} else if (type == "directional") {
				light.light_flags = LIGHT_DIRECTIONAL | LIGHT_FLAG_DELTA | LIGHT_FLAG_DELTA_DIRECTION;
			} else {
				LUMEN_WARN("Unknown analytical light type '%.*s' . Defaulting to point light", (int)type.size,
						   type.data);
				light.light_flags = LIGHT_POINT | LIGHT_FLAG_FINITE | LIGHT_FLAG_DELTA | LIGHT_FLAG_DELTA_POSITION;
			}
		}
	}

	LUMEN_ASSERT(meshes_node, "Meshes node not found in Lumen scene");

	// Meshes
	mesh_idx = 0;
	u64 num_emissives = 0;
	for (LumenNode* mesh_node = meshes_node->child; mesh_node; mesh_node = next_node(mesh_node), mesh_idx++) {
		const lm::String relative_mesh_file = get_or_default_str(get_node(mesh_node, "name"), "");
		LumenNode* materials_refs_node = get_node(mesh_node, "materials");
		LumenNode* transforms_node = get_node(mesh_node, "transforms");
		u32 material_entire_mesh_idx = U32_MAX;
		if (materials_refs_node) {
			for (LumenNode* mat_ref_node = materials_refs_node->child; mat_ref_node;
				 mat_ref_node = next_node(mat_ref_node)) {
				lm::String mat_name = get_str(get_node(mat_ref_node, "name"));
				auto* material_entry = material_map.find(mat_name);
				if (material_entry) {
					u32 mat_idx = material_entry->value;
					LumenNode* refs_node = get_node(mat_ref_node, "refs");
					if (refs_node && refs_node->num_list_items) {
						lm::FixedArray<lm::String> refs = get_str_list(scratch.arena, get_node(mat_ref_node, "refs"));
						for (const lm::String& ref : refs) {
							materials_to_objects.insert(ref, mat_idx);
						}
					} else if (refs_node) {
						materials_to_objects.insert(get_str(refs_node), mat_idx);
					} else {
						// Material ref points to all the meshes in this file
						material_entire_mesh_idx = mat_idx;
						break;
					}
				} else {
					LUMEN_ERROR("Material %s not found in Lumen scene", mat_name.data);
				}
			}
		}
		lm::mat4 world_matrix = lm::mat4(1);
		if (transforms_node) {
			lm::vec3 translation = get_or_default_v3(get_node(transforms_node, "translation"), lm::vec3(0));
			lm::vec3 rotation = lm::radians(get_or_default_v3(get_node(transforms_node, "rotation"), lm::vec3(0)));
			lm::vec3 scale = get_or_default_v3(get_node(transforms_node, "scale"), lm::vec3(1));
			world_matrix = lm::translate(world_matrix, translation);
			world_matrix = lm::rotate(world_matrix, rotation.x, lm::vec3(1, 0, 0));
			world_matrix = lm::rotate(world_matrix, rotation.y, lm::vec3(0, 1, 0));
			world_matrix = lm::rotate(world_matrix, rotation.z, lm::vec3(0, 0, 1));
			world_matrix = lm::scale(world_matrix, scale);
		}
		// Load obj file
		fastObjMesh* obj = mesh_to_obj_map.find(mesh_idx)->value;
		for (u32 shape_idx = 0; shape_idx < obj->object_count; shape_idx++) {
			lm::vec3 min_vtx = lm::vec3(F32_MAX);
			lm::vec3 max_vtx = lm::vec3(F32_MIN);

			LumenPrimMesh& prim_mesh = _scene.prim_meshes.emplace_back();
			prim_mesh.name = lm::str_from_cstr(_arena_strings, obj->objects[shape_idx].name);
			prim_mesh.filename = relative_mesh_file;
			prim_mesh.vtx_offset = (u32)_scene.positions.size;
			prim_mesh.first_idx = (u32)_scene.indices.size;

			u32 index_offset = obj->objects[shape_idx].index_offset;
			u32 idx_cnt = 0;
			u32 vtx_cnt = 0;
			for (u32 i = 0; i < obj->objects[shape_idx].face_count; i++) {
				for (u32 j = 0; j < obj->face_vertices[obj->objects[shape_idx].face_offset + i]; j++) {
					fastObjIndex idx = obj->indices[index_offset + j];
					if (j >= 3) {
						_scene.positions.push_back(_scene.positions[prim_mesh.vtx_offset + vtx_cnt - 3]);
						_scene.positions.push_back(_scene.positions[prim_mesh.vtx_offset + vtx_cnt - 1]);

						_scene.normals.push_back(_scene.normals[prim_mesh.vtx_offset + vtx_cnt - 3]);
						_scene.normals.push_back(_scene.normals[prim_mesh.vtx_offset + vtx_cnt - 1]);

						_scene.texcoords0.push_back(_scene.texcoords0[prim_mesh.vtx_offset + vtx_cnt - 3]);
						_scene.texcoords0.push_back(_scene.texcoords0[prim_mesh.vtx_offset + vtx_cnt - 1]);

						_scene.indices.push_back(idx_cnt++);
						_scene.indices.push_back(idx_cnt++);
						vtx_cnt += 2;
					}
					// TODO: Indices might be unnecessary
					_scene.indices.push_back(idx_cnt++);
					_scene.positions.emplace_back(obj->positions[3 * idx.p + 0], obj->positions[3 * idx.p + 1],
												  obj->positions[3 * idx.p + 2]);
					_scene.normals.emplace_back(obj->normals[3 * idx.n + 0], obj->normals[3 * idx.n + 1],
												obj->normals[3 * idx.n + 2]);

					_scene.texcoords0.emplace_back(obj->texcoords[2 * idx.t + 0], obj->texcoords[2 * idx.t + 1]);

					min_vtx = lm::min(_scene.positions.back(), min_vtx);
					max_vtx = lm::max(_scene.positions.back(), max_vtx);
					++vtx_cnt;
				}
				index_offset += obj->face_vertices[obj->objects[shape_idx].face_offset + i];
			}
			assert(idx_cnt == vtx_cnt);
			prim_mesh.idx_count = idx_cnt;
			prim_mesh.vtx_count = vtx_cnt;
			prim_mesh.prim_idx = (u32)_scene.prim_meshes.size - 1;

			lm::String obj_name = lm::str_from_cstr(obj->objects[shape_idx].name);
			auto* entry = materials_to_objects.find(obj_name);
			if (entry) {
				prim_mesh.material_idx = entry->value;
			} else if (material_entire_mesh_idx != U32_MAX) {
				prim_mesh.material_idx = material_entire_mesh_idx;
			} else {
				prim_mesh.material_idx = 0;	 // Default material
			}

			lm::vec3 emissive_factor = _scene.materials[prim_mesh.material_idx].emissive_factor;
			if (emissive_factor.x > 0 || emissive_factor.y > 0 || emissive_factor.z > 0) {
				num_emissives++;
			}

			prim_mesh.min_pos = min_vtx;
			prim_mesh.max_pos = max_vtx;
			prim_mesh.world_matrix = world_matrix;
		}
	}
	// Free obj data
	for (const lm::HashMapEntry<u32, fastObjMesh*>& kv : mesh_to_obj_map) {
		fast_obj_destroy(kv.value);
	}
	// Camera
	CameraSettings& cam_settings = _scene.config.common.cam_settings;
	lm::camera_init(&_scene.camera, cam_settings.fov, 0.01f, 1000.0f, Window::aspect_ratio(), cam_settings.dir,
					cam_settings.pos, cam_settings.rotation);

	// Allocate lights array
	_scene.gpu_lights = lm::fixed_array_create<Light>(_arena_scene, _scene.analytical_lights.capacity + num_emissives);
	// Compute scene dimensions and fill in lights array
	lm::BBox scene_bbox;
	for (const LumenPrimMesh& prim_mesh : _scene.prim_meshes) {
		lm::BBox mesh_bbox(prim_mesh.min_pos, prim_mesh.max_pos);
		lm::bbox_transform(mesh_bbox, prim_mesh.world_matrix);
		lm::bbox_insert(scene_bbox, mesh_bbox);
		lm::vec3 emissive_factor = _scene.materials[prim_mesh.material_idx].emissive_factor;

		if (emissive_factor.x > 0 || emissive_factor.y > 0 || emissive_factor.z > 0) {
			Light& light = _scene.gpu_lights.emplace_back();
			light.world_matrix = prim_mesh.world_matrix;
			light.num_triangles = prim_mesh.idx_count / 3;
			light.prim_mesh_idx = prim_mesh.prim_idx;
			light.material_idx = prim_mesh.material_idx;
			light.light_flags = LIGHT_AREA | LIGHT_FLAG_FINITE;
			if (_scene.materials[prim_mesh.material_idx].emission_two_sided != 0) {
				light.light_flags |= LIGHT_FLAG_TWO_SIDED;
			}
			light.L = emissive_factor;
		}
	}

	if (scene_bbox.is_empty()) {
		lm::bbox_insert(scene_bbox, lm::vec3(-1.0f));
		lm::bbox_insert(scene_bbox, lm::vec3(1.0f));
	}
	_scene.dimensions.min = scene_bbox.min();
	_scene.dimensions.max = scene_bbox.max();
	_scene.dimensions.size = scene_bbox.extents();
	_scene.dimensions.center = scene_bbox.center();
	_scene.dimensions.radius = scene_bbox.radius();

	// Scene lights
	for (u64 i = 0; i < _scene.analytical_lights.size; i++) {
		AnalyticalLight& l = _scene.analytical_lights[i];
		Light light{};
		light.L = l.L;
		light.light_flags = l.light_flags;
		light.pos = l.pos;
		light.to = l.to;
		light.cos_inner = cos(lm::radians(l.inner_angle));
		light.cos_outer = cos(lm::radians(l.outer_angle));
		light.world_radius = _scene.dimensions.radius;
		light.world_center = _scene.dimensions.center;
		if ((l.light_flags & LIGHT_DIRECTIONAL) == LIGHT_DIRECTIONAL) {
			LUMEN_ASSERT(_scene.dir_light_idx == -1, "Only one directional light supported");
			_scene.dir_light_idx = (u32)_scene.gpu_lights.size;
		}
		_scene.gpu_lights.emplace_back(light);
	}
}
static void add_default_texture() {
	u8 nil[4] = {0, 0, 0, 0};
	_scene.scene_textures.push_back(
		prm::get_texture({.name = CSTR("Default Scene Texture"),
						  .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
						  .dimensions = {1, 1, 1},
						  .format = VK_FORMAT_R8G8B8A8_SRGB,
						  .data = {.data = nil, .size = sizeof(nil)},
						  .sampler = _scene.scene_texture_sampler}));
}

static bool scene_has_bsdf_type(u32 bsdf_type) { return (_scene.bsdf_types & bsdf_type) == bsdf_type; }

void config_init(const lm::String& integrator_name, const SceneCommon& common_config, LumenNode* integrator_node) {
	lm::ScratchArena scratch = _arena_strings;
	lm::String name = lm::str_to_lower(scratch.arena, integrator_name);
	_scene.config.common = common_config;
	if (name == "path") {
		_scene.config.type = INTEGRATOR_PATH;
		_scene.config.settings.path = {};
	} else if (name == "bdpt") {
		_scene.config.type = INTEGRATOR_BDPT;
		_scene.config.settings.bdpt = {};
	} else if (name == "sppm") {
		_scene.config.type = INTEGRATOR_SPPM;
		_scene.config.settings.sppm = {};
		get_or_default_f_2(get_node(integrator_node, "base_radius"), _scene.config.settings.sppm.base_radius);
	} else if (name == "vcm") {
		_scene.config.type = INTEGRATOR_VCM;
		_scene.config.settings.vcm = {};
		get_or_default_bool_2(get_node(integrator_node, "enable_vm"), _scene.config.settings.vcm.enable_vm);
		get_or_default_f_2(get_node(integrator_node, "radius_factor"), _scene.config.settings.vcm.radius_factor);
	} else if (name == "pssmlt") {
		_scene.config.type = INTEGRATOR_PSSMLT;
		_scene.config.settings.pssmlt = {};
		PSSMLTConfig& pssmlt_config = _scene.config.settings.pssmlt;
		get_or_default_f_2(get_node(integrator_node, "mutations_per_pixel"), pssmlt_config.mutations_per_pixel);
		get_or_default_u_2(get_node(integrator_node, "num_mlt_threads"), pssmlt_config.num_mlt_threads);
		get_or_default_u_2(get_node(integrator_node, "num_bootstrap_samples"), pssmlt_config.num_bootstrap_samples);
	} else if (name == "smlt") {
		_scene.config.type = INTEGRATOR_SMLT;
		_scene.config.settings.smlt = {};
		SMLTConfig& smlt_config = _scene.config.settings.smlt;
		get_or_default_f_2(get_node(integrator_node, "mutations_per_pixel"), smlt_config.mutations_per_pixel);
		get_or_default_u_2(get_node(integrator_node, "num_mlt_threads"), smlt_config.num_mlt_threads);
		get_or_default_u_2(get_node(integrator_node, "num_bootstrap_samples"), smlt_config.num_bootstrap_samples);
	} else if (name == "vcmmlt") {
		_scene.config.type = INTEGRATOR_VCMMLT;
		_scene.config.settings.vcmmlt = {};
		VCMMLTConfig& vcmmlt_config = _scene.config.settings.vcmmlt;
		get_or_default_f_2(get_node(integrator_node, "mutations_per_pixel"), vcmmlt_config.mutations_per_pixel);
		get_or_default_u_2(get_node(integrator_node, "num_mlt_threads"), vcmmlt_config.num_mlt_threads);
		get_or_default_u_2(get_node(integrator_node, "num_bootstrap_samples"), vcmmlt_config.num_bootstrap_samples);
		get_or_default_f_2(get_node(integrator_node, "radius_factor"), vcmmlt_config.radius_factor);
		get_or_default_bool_2(get_node(integrator_node, "enable_vm"), vcmmlt_config.enable_vm);
		get_or_default_bool_2(get_node(integrator_node, "alternate"), vcmmlt_config.alternate);
		get_or_default_bool_2(get_node(integrator_node, "light_first"), vcmmlt_config.light_first);
	} else if (name == "restir") {
		_scene.config.type = INTEGRATOR_RESTIR;
		_scene.config.settings.restir = {};
	} else if (name == "restirgi") {
		_scene.config.type = INTEGRATOR_RESTIRGI;
		_scene.config.settings.restirgi = {};
	} else if (name == "restirpt") {
		_scene.config.type = INTEGRATOR_RESTIRPT;
		_scene.config.settings.restirpt = {};
	} else if (name == "ddgi") {
		_scene.config.type = INTEGRATOR_DDGI;
		_scene.config.settings.ddgi = {};
	} else if (name == "ircache") {
		_scene.config.type = INTEGRATOR_IRCACHE;
		_scene.config.settings.ircache = {};
	}
}

void load(const lm::String& path) {
	assert(path.is_cstr());
	os::FileHandle file_handle = os::file_open(path, os::AccessFlag_Read);
	if (file_handle == 0) {
		LUMEN_ERROR("Failed to open Lumen scene file: %s", path.data);
	}
	if (!_arena_scene) {
		_arena_scene = lm::arena_create(CSTR("Scene Arena"), GB(16), MB(16));
		_arena_strings = lm::arena_create(CSTR("Scene Strings Arena"), MB(16), MB(1));
	}
	os::FileProperties props = os::file_properties(file_handle);
	lm::String file_content = lm::str_reserve(_arena_strings, props.size + 1);
	os::file_read(file_handle, file_content.data);
	file_content.data[props.size] = '\0';
	os::file_close(file_handle);
	LumenNode* root = file_parse(file_content);
	if (!root) {
		LUMEN_ERROR("Failed to parse the file %s", path.data);
	}
	// TODO: Error check?
	lm::String path_root = lm::str_substr(path, 0, lm::str_rfind_any(path, "/\\") + 1);
	scene_init(path_root, root);

	u64 light_triangle_count = 0;
	for (const Light& light : _scene.gpu_lights) {
		if ((light.light_flags & LIGHT_TYPE_MASK) == LIGHT_AREA) {
			light_triangle_count += light.num_triangles;
		}
	}
	_scene.light_triangle_cdf = lm::fixed_array_create<LightTriangleCDF>(_arena_scene, light_triangle_count);
	_scene.emitter_light_indices = lm::fixed_array_create<u32>(_arena_scene, _scene.prim_meshes.size);
	for (u64 i = 0; i < _scene.prim_meshes.size; i++) {
		_scene.emitter_light_indices.push_back(INVALID_LIGHT_INDEX);
	}

	for (u32 light_idx = 0; light_idx < _scene.gpu_lights.size; light_idx++) {
		Light& l = _scene.gpu_lights[light_idx];
		if ((l.light_flags & LIGHT_TYPE_MASK) == LIGHT_AREA) {
			const LumenPrimMesh& pm = _scene.prim_meshes[l.prim_mesh_idx];
			l.world_matrix = pm.world_matrix;
			l.triangle_cdf_offset = (u32)_scene.light_triangle_cdf.size;
			l.material_idx = pm.material_idx;
			_scene.emitter_light_indices[l.prim_mesh_idx] = light_idx;
			f32 mesh_area = 0.0f;
			u32 idx_base_offset = pm.first_idx;
			u32 vtx_offset = pm.vtx_offset;
			for (u32 i = 0; i < l.num_triangles; i++) {
				u32 idx_offset = idx_base_offset + 3 * i;
				lm::ivec3 ind = {_scene.indices[idx_offset], _scene.indices[idx_offset + 1],
								 _scene.indices[idx_offset + 2]};
				ind += lm::ivec3(vtx_offset);
				const vec3 v0 = pm.world_matrix * lm::vec4(_scene.positions[ind.x], 1.0);
				const vec3 v1 = pm.world_matrix * lm::vec4(_scene.positions[ind.y], 1.0);
				const vec3 v2 = pm.world_matrix * lm::vec4(_scene.positions[ind.z], 1.0);
				f32 area = 0.5f * lm::length(lm::cross(v1 - v0, v2 - v0));
				mesh_area += area;
				_scene.light_triangle_cdf.push_back({mesh_area, i});
			}
			l.mesh_area = mesh_area;
		}
	}
	////////////////////////////
	// --- GPU memory allocation ---

	_scene.mesh_lights_buffer = prm::get_buffer({.name = CSTR("Mesh Lights Buffer"),
												 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
												 .memory_type = vk::BUFFER_TYPE_GPU,
												 .size = lm::max(_scene.gpu_lights.size, (u64)1) * sizeof(Light),
												 .data = _scene.gpu_lights.data});
	_scene.light_triangle_cdf_buffer =
		prm::get_buffer({.name = CSTR("Light Triangle CDF Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = lm::max(_scene.light_triangle_cdf.size, (u64)1) * sizeof(LightTriangleCDF),
						 .data = _scene.light_triangle_cdf.data});
	_scene.emitter_light_indices_buffer =
		prm::get_buffer({.name = CSTR("Emitter Light Indices Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = lm::max(_scene.emitter_light_indices.size, (u64)1) * sizeof(u32),
						 .data = _scene.emitter_light_indices.data});

	_scene.index_buffer =
		prm::get_buffer({.name = CSTR("Index Buffer"),
						 .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
								  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = _scene.indices.size * sizeof(u32),
						 .data = _scene.indices.data});

	_scene.materials_buffer =
		prm::get_buffer({.name = CSTR("Materials Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = _scene.materials.size * sizeof(Material),
						 .data = _scene.materials.data});

	{
		lm::ScratchArena scratch = _arena_scene;
		// GPU friendlier data
		lm::FixedArray<PrimInfo> prim_lookup = lm::fixed_array_create<PrimInfo>(scratch.arena, _scene.prim_meshes.size);
		lm::FixedArray<Vertex> vertices = lm::fixed_array_create<Vertex>(scratch.arena, _scene.positions.size);
		lm::FixedArray<InstanceTransform> transformations =
			lm::fixed_array_create<InstanceTransform>(scratch.arena, _scene.prim_meshes.size);
		prim_lookup.size = _scene.prim_meshes.size;
		transformations.size = _scene.prim_meshes.size;

		for (const LumenPrimMesh& pm : _scene.prim_meshes) {
			assert(pm.prim_idx < prim_lookup.size);
			PrimInfo& gpu_pm = prim_lookup[pm.prim_idx];
			gpu_pm.index_offset = pm.first_idx;
			gpu_pm.vertex_offset = pm.vtx_offset;
			gpu_pm.material_index = pm.material_idx;

			const lm::mat4 normal_to_world = lm::transpose(lm::inverse(pm.world_matrix));
			InstanceTransform& transform = transformations[pm.prim_idx];
			transform.object_to_world_x = lm::vec3(pm.world_matrix[0]);
			transform.object_to_world_y = lm::vec3(pm.world_matrix[1]);
			transform.object_to_world_z = lm::vec3(pm.world_matrix[2]);
			transform.object_to_world_translation = lm::vec3(pm.world_matrix[3]);
			transform.normal_to_world_x = lm::vec3(normal_to_world[0]);
			transform.normal_to_world_y = lm::vec3(normal_to_world[1]);
			transform.normal_to_world_z = lm::vec3(normal_to_world[2]);
		}
		for (u64 i = 0; i < _scene.positions.size; i++) {
			Vertex& v = vertices.emplace_back();
			v.pos = _scene.positions[i];
			v.normal = _scene.normals[i];
			v.uv0 = _scene.texcoords0[i];
		}
		_scene.prim_lookup_buffer =
			prm::get_buffer({.name = CSTR("Prim Lookup Buffer"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = prim_lookup.size * sizeof(PrimInfo),
							 .data = prim_lookup.data});

		_scene.vertex_buffer =
			prm::get_buffer({.name = CSTR("Compact Vertices Buffer"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = vertices.size * sizeof(vertices[0]),
							 .data = vertices.data});

		_scene.transformations_buffer =
			prm::get_buffer({.name = CSTR("Instance Transformations Buffer"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = transformations.size * sizeof(InstanceTransform),
							 .data = transformations.data});
	}
	// Create a sampler for textures
	VkSamplerCreateInfo sampler_ci = vk::sampler();
	sampler_ci.minFilter = VK_FILTER_LINEAR;
	sampler_ci.magFilter = VK_FILTER_LINEAR;
	sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_ci.maxLod = FLT_MAX;
	vk::check(vkCreateSampler(vk::context().device, &sampler_ci, nullptr, &_scene.scene_texture_sampler));

	if (!_scene.textures.size) {
		add_default_texture();
	} else {
		for (const TextureRef& texture_path : _scene.textures) {
			lm::ScratchArena scratch = _arena_strings;
			i32 x, y, n;
			lm::String img_path = lm::str_concat(scratch.arena, path_root, texture_path.relative_path);
			unsigned char* data = stbi_load(img_path.data, &x, &y, &n, 4);

			_scene.scene_textures.push_back(
				prm::get_texture({.name = texture_path.allocation_name,
								  .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
								  .dimensions = {(u32)x, (u32)y, 1},
								  .format = VK_FORMAT_R8G8B8A8_SRGB,
								  .data = {.data = data, .size = u64(x * y * 4)},
								  .sampler = _scene.scene_texture_sampler}));
			stbi_image_free(data);
		}
	}

	u64 total_used = 0;
	u64 total_allocated = 0;
	arena_get_stats(_arena_scene, total_used, total_allocated);
	arena_get_stats(_arena_strings, total_used, total_allocated);
	f64 MB = 1024.0 * 1024.0;
	LUMEN_ASSERT(!_arena_scene->next, "Scene arena should be a single block");
	LUMEN_INFO("Scene Arena: Total memory used: %.2f MB / allocated: %.2f MB (%.2f%%)", total_used / MB,
			   total_allocated / MB, (f64)100.0 * total_used / total_allocated);

	rg::add_global_macro(
		vk::ShaderMacro("ENABLE_DIFFUSE", scene_has_bsdf_type(BSDF_TYPE_DIFFUSE), /* visible = */ false));
	rg::add_global_macro(
		vk::ShaderMacro("ENABLE_MIRROR", scene_has_bsdf_type(BSDF_TYPE_MIRROR), /* visible = */ false));
	rg::add_global_macro(vk::ShaderMacro("ENABLE_GLASS", scene_has_bsdf_type(BSDF_TYPE_GLASS), /* visible = */ false));
	rg::add_global_macro(
		vk::ShaderMacro("ENABLE_DIELECTRIC", scene_has_bsdf_type(BSDF_TYPE_DIELECTRIC), /* visible = */ false));
	rg::add_global_macro(
		vk::ShaderMacro("ENABLE_CONDUCTOR", scene_has_bsdf_type(BSDF_TYPE_CONDUCTOR), /* visible = */ false));
	rg::add_global_macro(
		vk::ShaderMacro("ENABLE_PRINCIPLED", scene_has_bsdf_type(BSDF_TYPE_PRINCIPLED), /* visible = */ false));
}

void write() {
	lm::ScratchArena scratch = _arena_strings;
	LumenNode* root = node_create(scratch.arena, "scene");

	// Integrator settings
	LumenNode* integrator_node = node_create(scratch.arena, "integrator");
	const SceneCommon& common_config = _scene.config.common;
	lm::String integrator_name = lm::str_to_lower(scratch.arena, common_config.integrator_name);
	add_leaf_node(scratch.arena, integrator_node, "type", integrator_name);
	lm::String path_length = lm::str_from_u32(scratch.arena, common_config.path_length);
	add_leaf_node(scratch.arena, integrator_node, "path_length", path_length);
	lm::String sky_col = str_from_vec3(scratch.arena, common_config.sky_col);
	add_leaf_node(scratch.arena, integrator_node, "sky_col", sky_col);
	if (integrator_name == "sppm") {
		const SPPMConfig& sppm_config = _scene.config.settings.sppm;
		lm::String base_radius = lm::str_from_f32(scratch.arena, sppm_config.base_radius);
		add_leaf_node(scratch.arena, integrator_node, "base_radius", base_radius);
	} else if (integrator_name == "vcm") {
		const VCMConfig& vcm_config = _scene.config.settings.vcm;
		lm::String enable_vm = lm::str_from_u32(scratch.arena, vcm_config.enable_vm ? 1 : 0);
		lm::String radius_factor = lm::str_from_f32(scratch.arena, vcm_config.radius_factor);
		add_leaf_node(scratch.arena, integrator_node, "enable_vm", enable_vm);
		add_leaf_node(scratch.arena, integrator_node, "radius_factor", radius_factor);
	} else if (integrator_name == "pssmlt") {
		const PSSMLTConfig& pssmlt_config = _scene.config.settings.pssmlt;
		lm::String mutations_per_pixel = lm::str_from_f32(scratch.arena, pssmlt_config.mutations_per_pixel);
		lm::String num_mlt_threads = lm::str_from_u32(scratch.arena, pssmlt_config.num_mlt_threads);
		lm::String num_bootstrap_samples = lm::str_from_u32(scratch.arena, pssmlt_config.num_bootstrap_samples);
		add_leaf_node(scratch.arena, integrator_node, "mutations_per_pixel", mutations_per_pixel);
		add_leaf_node(scratch.arena, integrator_node, "num_mlt_threads", num_mlt_threads);
		add_leaf_node(scratch.arena, integrator_node, "num_bootstrap_samples", num_bootstrap_samples);
	} else if (integrator_name == "smlt") {
		const SMLTConfig& smlt_config = _scene.config.settings.smlt;
		lm::String mutations_per_pixel = lm::str_from_f32(scratch.arena, smlt_config.mutations_per_pixel);
		lm::String num_mlt_threads = lm::str_from_u32(scratch.arena, smlt_config.num_mlt_threads);
		lm::String num_bootstrap_samples = lm::str_from_u32(scratch.arena, smlt_config.num_bootstrap_samples);
		add_leaf_node(scratch.arena, integrator_node, "mutations_per_pixel", mutations_per_pixel);
		add_leaf_node(scratch.arena, integrator_node, "num_mlt_threads", num_mlt_threads);
		add_leaf_node(scratch.arena, integrator_node, "num_bootstrap_samples", num_bootstrap_samples);
	} else if (integrator_name == "vcmmlt") {
		const VCMMLTConfig& vcmmlt_config = _scene.config.settings.vcmmlt;
		lm::String mutations_per_pixel = lm::str_from_f32(scratch.arena, vcmmlt_config.mutations_per_pixel);
		lm::String num_mlt_threads = lm::str_from_u32(scratch.arena, vcmmlt_config.num_mlt_threads);
		lm::String num_bootstrap_samples = lm::str_from_u32(scratch.arena, vcmmlt_config.num_bootstrap_samples);
		lm::String radius_factor = lm::str_from_f32(scratch.arena, vcmmlt_config.radius_factor);
		lm::String enable_vm = lm::str_from_u32(scratch.arena, vcmmlt_config.enable_vm ? 1 : 0);
		lm::String alternate = lm::str_from_u32(scratch.arena, vcmmlt_config.alternate ? 1 : 0);
		lm::String light_first = lm::str_from_u32(scratch.arena, vcmmlt_config.light_first ? 1 : 0);
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
	LumenNode* textures_node = node_create(scratch.arena, "textures");
	for (const TextureRef& texture_ref : _scene.textures) {
		LumenNode* texture_prop = node_create(scratch.arena, "-");
		add_leaf_node(scratch.arena, texture_prop, "name", texture_ref.name);
		add_leaf_node(scratch.arena, texture_prop, "file", texture_ref.relative_path);
		add_child_node(textures_node, texture_prop);
	}
	add_child_node(root, textures_node);
	// Scene materials
	LumenNode* bsdfs_node = node_create(scratch.arena, "bsdfs");
	for (u32 i = 0; i < _scene.materials.size; i++) {
		LumenNode* bsdf_prop = node_create(scratch.arena, "-");
		const Material& mat = _scene.materials[i];
		auto* material_entry = _scene.material_idx_to_name.find(i);
		assert(material_entry);
		add_leaf_node(scratch.arena, bsdf_prop, "name", material_entry->value);
		add_leaf_node(scratch.arena, bsdf_prop, "type", get_material_type(mat));
		lm::String albedo = str_from_vec3(scratch.arena, mat.albedo);
		add_leaf_node(scratch.arena, bsdf_prop, "albedo", albedo);
		if (lm::any(lm::greaterThan(mat.emissive_factor, lm::vec3(0.0f)))) {
			lm::String emissive_factor = str_from_vec3(scratch.arena, mat.emissive_factor);
			add_leaf_node(scratch.arena, bsdf_prop, "emissive_factor", emissive_factor);
		}
		if (mat.texture_id != -1) {
			add_leaf_node(scratch.arena, bsdf_prop, "texture", _scene.textures[mat.texture_id].name);
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
				lm::vec3 r_num = mat.albedo - lm::vec3(1.0f);
				lm::vec3 r_denom = mat.albedo + lm::vec3(1.0f);
				lm::vec3 reflectivity = (r_num * r_num + mat.k * mat.k) / (r_denom * r_denom + mat.k * mat.k);
				lm::vec3 n_min = (1.0f - reflectivity) / (1.0f + reflectivity);
				lm::vec3 n_max = (1.0f + lm::sqrt(reflectivity)) / (1.0f - lm::sqrt(reflectivity));

				lm::vec3 edge_tint;
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
	if (!_scene.analytical_lights.empty()) {
		LumenNode* lights_node = node_create(scratch.arena, "lights");
		for (const AnalyticalLight& light : _scene.analytical_lights) {
			LumenNode* light_node = node_create(scratch.arena, "-");

			add_leaf_node(scratch.arena, light_node, "type", get_light_type_str(light));
			add_leaf_node(scratch.arena, light_node, "L", str_from_vec3(scratch.arena, light.L));
			add_leaf_node(scratch.arena, light_node, "pos", str_from_vec3(scratch.arena, light.pos));
			add_leaf_node(scratch.arena, light_node, "dir", str_from_vec3(scratch.arena, light.to));
			if ((light.light_flags & LIGHT_TYPE_MASK) == LIGHT_SPOT) {
				add_leaf_node(scratch.arena, light_node, "inner_angle",
							  lm::str_from_f32(scratch.arena, light.inner_angle));
				add_leaf_node(scratch.arena, light_node, "outer_angle",
							  lm::str_from_f32(scratch.arena, light.outer_angle));
			}
			add_child_node(lights_node, light_node);
		}
		add_child_node(root, lights_node);
	}

	// Camera settings
	LumenNode* camera_node = node_create(scratch.arena, "camera");
	add_leaf_node(scratch.arena, camera_node, "fov", lm::str_from_f32(scratch.arena, _scene.camera.fov));
	add_leaf_node(scratch.arena, camera_node, "position", str_from_vec3(scratch.arena, _scene.camera.position));
	add_leaf_node(scratch.arena, camera_node, "rotation", str_from_vec3(scratch.arena, _scene.camera.rotation));
	add_leaf_node(scratch.arena, camera_node, "dir", str_from_vec3(scratch.arena, _scene.camera.direction));
	add_child_node(root, camera_node);

	// Mesh and material mappings
	if (!_scene.prim_meshes.empty()) {
		// File name to mesh/sub-mesh mapping
		auto mesh_mappings = lm::hash_map_create<lm::String, lm::FixedArray<LumenPrimMesh*>>(scratch.arena);

		for (LumenPrimMesh& mesh : _scene.prim_meshes) {
			auto* entry = mesh_mappings.get_or_create(mesh.filename);
			if (!entry->value.initialized()) {
				entry->value = lm::fixed_array_create<LumenPrimMesh*>(scratch.arena, _scene.prim_meshes.size);
			}
			entry->value.push_back(&mesh);
		}

		LumenNode* meshes_node = node_create(scratch.arena, "mesh");
		for (const lm::HashMapEntry<lm::String, lm::FixedArray<LumenPrimMesh*>>& mesh_mapping : mesh_mappings) {
			LumenNode* mesh_node = node_create(scratch.arena, "-");
			add_leaf_node(scratch.arena, mesh_node, "name", mesh_mapping.key);
			LumenNode* materials_node = node_create(scratch.arena, "materials");
			LumenNode* transforms_node = node_create(scratch.arena, "transforms");
			add_child_node(mesh_node, materials_node);
			bool has_transform = false;
			for (LumenPrimMesh* mesh : mesh_mapping.value) {
				LumenNode* mesh_to_material_node = node_create(scratch.arena, "-");
				if (!mesh->name.empty()) {
					add_leaf_node(scratch.arena, mesh_to_material_node, "refs", mesh->name);
				}

				auto* material_entry = _scene.material_idx_to_name.find(mesh->material_idx);
				assert(material_entry);
				add_leaf_node(scratch.arena, mesh_to_material_node, "name", material_entry->value);
				add_child_node(materials_node, mesh_to_material_node);

				lm::vec3 scale;
				lm::quat q;
				lm::vec3 translation;
				lm::vec3 skew;
				lm::vec4 perspective;
				lm::decompose(mesh->world_matrix, scale, q, translation, skew, perspective);
				lm::vec3 rot{};
				lm::extract_euler_angle_xyz(lm::to_mat4(q), rot.x, rot.y, rot.z);
				if (translation != lm::vec3(0.0)) {
					add_leaf_node(scratch.arena, transforms_node, "translation",
								  str_from_vec3(scratch.arena, translation));
					has_transform = true;
				}
				if (rot != lm::vec3(0.0)) {
					add_leaf_node(scratch.arena, transforms_node, "rotation",
								  str_from_vec3(scratch.arena, lm::degrees(rot)));
					has_transform = true;
				}
				if (scale != lm::vec3(1.0)) {
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
}

void destroy() {
	std::initializer_list<vk::Buffer*> buffer_list = {_scene.index_buffer,
													  _scene.vertex_buffer,
													  _scene.materials_buffer,
													  _scene.prim_lookup_buffer,
													  _scene.transformations_buffer,
													  _scene.mesh_lights_buffer,
													  _scene.light_triangle_cdf_buffer,
													  _scene.emitter_light_indices_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
	for (vk::Texture* tex : _scene.scene_textures) {
		prm::remove(tex);
	}
	vkDestroySampler(vk::context().device, _scene.scene_texture_sampler, nullptr);
}

Scene* get() { return &_scene; }
}  // namespace scene
