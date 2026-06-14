#include "Shader.h"
#include "RenderGraph.h"
#include "Framework/Base/OS.h"
#include <spirv_cross/spirv_cross_c.h>
#include <shaderc/shaderc.h>

constexpr u64 MAX_SHADER_WORKER_ARENAS = 64;

struct ShaderThreadState {
	lm::Arena* arena = nullptr;
	// Reused for the worker's lifetime; process shutdown reclaims them.
	shaderc_compiler_t compiler = nullptr;
	spvc_context spirv_context = nullptr;
};

struct ShaderIncludeContext {
	lm::Arena* arena;
};

static thread_local ShaderThreadState _shader_thread_state;

// We need this to be able to reset all threads' arenas
static lm::SmallArray<lm::Arena*, MAX_SHADER_WORKER_ARENAS> _shader_worker_arenas;
static os::Mutex _shader_worker_arenas_mutex;

static u64 string_content_size(const lm::String& string) {
	return string.size && string.data[string.size - 1] == '\0' ? string.size - 1 : string.size;
}

static lm::String string_content(const lm::String& string) { return {string.data, string_content_size(string)}; }

static lm::Arena* get_shader_arena() {
	if (!_shader_thread_state.arena) {
		os::ScopedLock lock(_shader_worker_arenas_mutex);
		LUMEN_ASSERT(_shader_worker_arenas.size < _shader_worker_arenas.capacity(),
					 "Exceeded maximum shader worker arena count");
		_shader_thread_state.arena = lm::arena_create(CSTR("Shader Arena"), MB(16), MB(1));
		_shader_worker_arenas.push_back(_shader_thread_state.arena);
	}
	return _shader_thread_state.arena;
}

static shaderc_compiler_t get_shader_compiler() {
	if (!_shader_thread_state.compiler) {
		_shader_thread_state.compiler = shaderc_compiler_initialize();
	}
	return _shader_thread_state.compiler;
}

static spvc_context get_spirv_context() {
	if (!_shader_thread_state.spirv_context &&
		spvc_context_create(&_shader_thread_state.spirv_context) != SPVC_SUCCESS) {
		lm::log(lm::LOG_ERROR, "Failed to create SPIRV-Cross context");
	}
	return _shader_thread_state.spirv_context;
}

static bool spvc_check(spvc_context context, spvc_result result, const char* operation) {
	if (result == SPVC_SUCCESS) {
		return true;
	}
	const char* error = context ? spvc_context_get_last_error_string(context) : "";
	lm::log(lm::LOG_ERROR, "SPIRV-Cross %s failed: %s", operation, error ? error : "");
	return false;
}

struct SpvcResourceList {
	const spvc_reflected_resource* data = nullptr;
	size_t size = 0;
};

static bool get_spvc_resources(spvc_context context, spvc_resources resources, spvc_resource_type type,
							   SpvcResourceList& list) {
	return spvc_check(context, spvc_resources_get_resource_list_for_type(resources, type, &list.data, &list.size),
					  "resource lookup");
}

static bool create_spvc_compiler(const u32* code, u64 code_size, spvc_context& context, spvc_compiler& compiler) {
	context = get_spirv_context();
	if (!context) {
		return false;
	}

	spvc_context_release_allocations(context);
	spvc_parsed_ir parsed_ir = nullptr;
	if (!spvc_check(context, spvc_context_parse_spirv(context, code, code_size, &parsed_ir), "SPIR-V parse")) {
		return false;
	}
	return spvc_check(context,
					  spvc_context_create_compiler(context, SPVC_BACKEND_NONE, parsed_ir,
												   SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler),
					  "compiler creation");
}

static bool read_file(lm::Arena* arena, const lm::String& path, lm::String& contents) {
	os::FileHandle file = os::file_open(path, os::AccessFlag_Read);
	if (!file) {
		return false;
	}

	const os::FileProperties properties = os::file_properties(file);
	contents = lm::str_reserve(arena, properties.size);
	const u64 bytes_read = os::file_read(file, contents.data, properties.size);
	os::file_close(file);
	return bytes_read == properties.size;
}

static lm::String path_join(lm::Arena* arena, const lm::String& directory, const lm::String& filename) {
	lm::String path = directory;
	if (!path.empty() && path[path.size - 1] != '/' && path[path.size - 1] != '\\') {
		path = lm::str_concat(arena, path, "/");
	}
	return lm::str_concat(arena, path, filename, /*cstr=*/true);
}

static shaderc_include_result* make_include_error(lm::Arena* arena, const char* message) {
	shaderc_include_result* result = (shaderc_include_result*)arena->allocate(sizeof(shaderc_include_result));
	result->source_name = "";
	result->source_name_length = 0;
	result->content = message;
	result->content_length = strlen(message);
	return result;
}

static shaderc_include_result* shader_include_resolve(void* user_data, const char* requested_source, int include_type,
													  const char* requesting_source, size_t) {
	ShaderIncludeContext* context = (ShaderIncludeContext*)user_data;
	lm::Arena* arena = context->arena;
	const lm::String requested = lm::str_from_cstr(requested_source);

	lm::String resolved_path;
	lm::String contents;
	if (include_type == shaderc_include_type_relative && requesting_source) {
		const lm::String requesting = lm::str_from_cstr(requesting_source);
		const u64 slash = lm::str_rfind_any(requesting, "/\\");
		if (slash != U64_MAX) {
			const lm::String directory = {requesting.data, slash};
			const lm::String relative_path = path_join(arena, directory, requested);
			if (read_file(arena, relative_path, contents)) {
				resolved_path = relative_path;
			}
		}
	}

	if (resolved_path.empty()) {
		const lm::String requested_path = lm::str_to_cstr(arena, requested);
		if (!read_file(arena, requested_path, contents)) {
			return make_include_error(arena, "Cannot find or open include file.");
		}
		resolved_path = requested_path;
	}

	shaderc_include_result* result = (shaderc_include_result*)arena->allocate(sizeof(shaderc_include_result));
	result->source_name = resolved_path.data;
	result->source_name_length = string_content_size(resolved_path);
	result->content = contents.data;
	result->content_length = contents.size;
	return result;
}

static void shader_include_release(void*, shaderc_include_result*) {}

static void add_macros(const vk::ShaderMacroArray& macros, shaderc_compile_options_t options, lm::Arena* arena) {
	for (const vk::ShaderMacro& macro : macros) {
		if (macro.name.empty()) {
			continue;
		}

		const u64 name_size = string_content_size(macro.name);
		if (macro.has_val) {
			const lm::String value = lm::str_from_s64(arena, macro.val);
			shaderc_compile_options_add_macro_definition(options, macro.name.data, name_size, value.data, value.size);
		} else {
			shaderc_compile_options_add_macro_definition(options, macro.name.data, name_size, nullptr, 0);
		}
	}
}

static shaderc_compilation_result_t compile_file(const lm::String& source_name, shaderc_shader_kind kind,
												 const lm::String& source, lm::RenderPass* pass,
												 lm::Arena* scratch_arena, bool optimize = false) {
	shaderc_compiler_t compiler = get_shader_compiler();
	shaderc_compile_options_t options = shaderc_compile_options_initialize();
	if (!compiler || !options) {
		if (options) {
			shaderc_compile_options_release(options);
		}
		return nullptr;
	}

	add_macros(pass->settings.macros, options, scratch_arena);
	add_macros(rg::global_macro_defines(), options, scratch_arena);
	if (optimize) {
		shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_size);
	}

	ShaderIncludeContext include_context = {.arena = scratch_arena};
	shaderc_compile_options_set_include_callbacks(options, shader_include_resolve, shader_include_release,
												  &include_context);
	shaderc_compile_options_set_target_spirv(options, shaderc_spirv_version_1_6);
	shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
	shaderc_compile_options_set_generate_debug_info(options);

	shaderc_compilation_result_t result =
		shaderc_compile_into_spv(compiler, source.data, source.size, kind, source_name.data, "main", options);
	shaderc_compile_options_release(options);
	return result;
}

static bool get_shader_kind(const lm::String& filename, shaderc_shader_kind& kind) {
	const lm::String name = string_content(filename);
	const u64 dot = lm::str_rfind(name, ".");
	if (dot == U64_MAX || dot + 1 >= name.size) {
		return false;
	}

	const lm::String extension = lm::str_substr(name, dot + 1, name.size - dot - 1);
	struct StageExtension {
		lm::String extension;
		shaderc_shader_kind kind;
	};
	static const StageExtension _stages[] = {
		{"vert", shaderc_vertex_shader},	   {"frag", shaderc_fragment_shader},
		{"comp", shaderc_compute_shader},	   {"geom", shaderc_geometry_shader},
		{"tesc", shaderc_tess_control_shader}, {"tese", shaderc_tess_evaluation_shader},
		{"rgen", shaderc_raygen_shader},	   {"rahit", shaderc_anyhit_shader},
		{"rchit", shaderc_closesthit_shader},  {"rmiss", shaderc_miss_shader},
		{"rint", shaderc_intersection_shader}, {"rcall", shaderc_callable_shader},
		{"task", shaderc_task_shader},		   {"mesh", shaderc_mesh_shader},
	};
	for (const StageExtension& stage : _stages) {
		if (extension == stage.extension) {
			kind = stage.kind;
			return true;
		}
	}
	return false;
}

template <typename T>
static lm::FixedArray<T> id_array_create(lm::Arena* arena, u32 id_bound) {
	lm::FixedArray<T> array = lm::fixed_array_create<T>(arena, id_bound);
	array.size = id_bound;
	return array;
}

namespace vk {
static bool get_vertex_input(spvc_basetype base_type, u32 vector_size, VertexInput& input) {
	struct VertexInputMapping {
		spvc_basetype base_type;
		u32 vector_size;
		VertexInput input;
	};
	static const VertexInputMapping _mappings[] = {
		{SPVC_BASETYPE_INT32, 1u, {VK_FORMAT_R32_SINT, (u32)sizeof(i32)}},
		{SPVC_BASETYPE_INT32, 2u, {VK_FORMAT_R32G32_SINT, 2 * (u32)sizeof(i32)}},
		{SPVC_BASETYPE_INT32, 3u, {VK_FORMAT_R32G32B32_SINT, 3 * (u32)sizeof(i32)}},
		{SPVC_BASETYPE_INT32, 4u, {VK_FORMAT_R32G32B32A32_SINT, 4 * (u32)sizeof(i32)}},
		{SPVC_BASETYPE_UINT32, 1u, {VK_FORMAT_R32_UINT, (u32)sizeof(u32)}},
		{SPVC_BASETYPE_UINT32, 2u, {VK_FORMAT_R32G32_UINT, 2 * (u32)sizeof(u32)}},
		{SPVC_BASETYPE_UINT32, 3u, {VK_FORMAT_R32G32B32_UINT, 3 * (u32)sizeof(u32)}},
		{SPVC_BASETYPE_UINT32, 4u, {VK_FORMAT_R32G32B32A32_UINT, 4 * (u32)sizeof(u32)}},
		{SPVC_BASETYPE_INT16, 1u, {VK_FORMAT_R16_SINT, (u32)sizeof(i16)}},
		{SPVC_BASETYPE_INT16, 2u, {VK_FORMAT_R16G16_SINT, 2 * (u32)sizeof(i16)}},
		{SPVC_BASETYPE_INT16, 3u, {VK_FORMAT_R16G16B16_SINT, 3 * (u32)sizeof(i16)}},
		{SPVC_BASETYPE_INT16, 4u, {VK_FORMAT_R16G16B16A16_SINT, 4 * (u32)sizeof(i16)}},
		{SPVC_BASETYPE_UINT16, 1u, {VK_FORMAT_R16_UINT, (u32)sizeof(u16)}},
		{SPVC_BASETYPE_UINT16, 2u, {VK_FORMAT_R16G16_UINT, 2 * (u32)sizeof(u16)}},
		{SPVC_BASETYPE_UINT16, 3u, {VK_FORMAT_R16G16B16_UINT, 3 * (u32)sizeof(u16)}},
		{SPVC_BASETYPE_UINT16, 4u, {VK_FORMAT_R16G16B16A16_UINT, 4 * (u32)sizeof(u16)}},
		{SPVC_BASETYPE_FP32, 1u, {VK_FORMAT_R32_SFLOAT, (u32)sizeof(f32)}},
		{SPVC_BASETYPE_FP32, 2u, {VK_FORMAT_R32G32_SFLOAT, 2 * (u32)sizeof(f32)}},
		{SPVC_BASETYPE_FP32, 3u, {VK_FORMAT_R32G32B32_SFLOAT, 3 * (u32)sizeof(f32)}},
		{SPVC_BASETYPE_FP32, 4u, {VK_FORMAT_R32G32B32A32_SFLOAT, 4 * (u32)sizeof(f32)}},
		{SPVC_BASETYPE_FP16, 1u, {VK_FORMAT_R16_SFLOAT, (u32)sizeof(u16)}},
		{SPVC_BASETYPE_FP16, 2u, {VK_FORMAT_R16G16_SFLOAT, 2 * (u32)sizeof(u16)}},
		{SPVC_BASETYPE_FP16, 3u, {VK_FORMAT_R16G16B16_SFLOAT, 3 * (u32)sizeof(u16)}},
		{SPVC_BASETYPE_FP16, 4u, {VK_FORMAT_R16G16B16A16_SFLOAT, 4 * (u32)sizeof(u16)}},
	};

	for (const VertexInputMapping& mapping : _mappings) {
		if (mapping.base_type == base_type && mapping.vector_size == vector_size) {
			input = mapping.input;
			return true;
		}
	}
	return false;
}

static VkShaderStageFlagBits get_shader_stage(SpvExecutionModel execution_model) {
	switch (execution_model) {
		case SpvExecutionModelVertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case SpvExecutionModelFragment:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case SpvExecutionModelGLCompute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		case SpvExecutionModelTaskNV:
			return VK_SHADER_STAGE_TASK_BIT_NV;
		case SpvExecutionModelMeshNV:
			return VK_SHADER_STAGE_MESH_BIT_NV;
		case SpvExecutionModelRayGenerationKHR:
			return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case SpvExecutionModelIntersectionKHR:
			return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case SpvExecutionModelAnyHitKHR:
			return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case SpvExecutionModelClosestHitKHR:
			return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case SpvExecutionModelMissKHR:
			return VK_SHADER_STAGE_MISS_BIT_KHR;
		case SpvExecutionModelCallableKHR:
			return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		default:
			assert(!"Unsupported execution model");
			return VkShaderStageFlagBits(0);
	}
}

static bool is_bound_buffer(u32 storage_class) { return storage_class == SpvStorageClassStorageBuffer; }

static bool is_buffer(u32 storage_class) {
	return storage_class == SpvStorageClassStorageBuffer || storage_class == SpvStorageClassPhysicalStorageBuffer;
}

static bool parse_spirv(spvc_context context, spvc_compiler compiler, Shader& shader, const u32* code, u64 code_size,
						lm::RenderPass* pass, lm::Arena* scratch_arena) {
	// Update active image and buffer status before walking individual instructions:
	// storage images can be read and/or written, sampled images are reads, and
	// active storage buffers must remain visible even if no access is inferred below.
	spvc_set active_variables = nullptr;
	spvc_resources active_resources = nullptr;
	if (!spvc_check(context, spvc_compiler_get_active_interface_variables(compiler, &active_variables),
					"active variable lookup") ||
		!spvc_check(context,
					spvc_compiler_create_shader_resources_for_active_variables(compiler, &active_resources,
																			  active_variables),
					"active resource reflection")) {
		return false;
	}

	SpvcResourceList resources;
	if (!get_spvc_resources(context, active_resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, resources)) {
		return false;
	}
	for (size_t i = 0; i < resources.size; i++) {
		const u32 binding = spvc_compiler_get_decoration(compiler, resources.data[i].id, SpvDecorationBinding);
		shader.resource_binding_map.insert(binding, BindingStatus{.read = true, .active = true});
	}
	if (!get_spvc_resources(context, active_resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, resources)) {
		return false;
	}
	for (size_t i = 0; i < resources.size; i++) {
		const spvc_reflected_resource& storage_image = resources.data[i];
		const u32 binding = spvc_compiler_get_decoration(compiler, storage_image.id, SpvDecorationBinding);
		BindingStatus& status = shader.resource_binding_map.get_or_create(binding)->value;
		status.read |= !spvc_compiler_has_decoration(compiler, storage_image.id, SpvDecorationNonReadable);
		status.write |= !spvc_compiler_has_decoration(compiler, storage_image.id, SpvDecorationNonWritable);
		status.active = true;
	}
	if (!get_spvc_resources(context, active_resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, resources)) {
		return false;
	}
	for (size_t i = 0; i < resources.size; i++) {
		const u32 binding = spvc_compiler_get_decoration(compiler, resources.data[i].id, SpvDecorationBinding);
		shader.resource_binding_map.get_or_create(binding)->value.active = true;
	}
	if (!get_spvc_resources(context, active_resources, SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE, resources)) {
		return false;
	}
	for (size_t i = 0; i < resources.size; i++) {
		const u32 set = spvc_compiler_get_decoration(compiler, resources.data[i].id, SpvDecorationDescriptorSet);
		LUMEN_ASSERT(set == 1, "Acceleration structure must be in descriptor set 1");
		shader.num_as_bindings++;
	}

	assert(code[0] == SpvMagicNumber);
	const u32 id_bound = code[3];
	struct Variable {
		u32 storage_class = 0;
		u32 type_id = 0;
		bool valid = false;
	};
	struct AccessChain {
		u32 base_ptr_id = 0;
		u32 offset_idx = 0;
		bool valid = false;
	};
	struct PointerType {
		u32 pointee_type_id = 0;
		bool valid = false;
	};

	auto access_chains = id_array_create<AccessChain>(scratch_arena, id_bound);
	auto variables = id_array_create<Variable>(scratch_arena, id_bound);
	auto pointer_types = id_array_create<PointerType>(scratch_arena, id_bound);
	auto load_ids = id_array_create<u32>(scratch_arena, id_bound);  // Result ID -> pointer ID
	auto constant_ids = id_array_create<bool>(scratch_arena, id_bound);
	// Pointer ID -> registered buffer pointer name.
	auto buffer_pointer_names = id_array_create<lm::String>(scratch_arena, id_bound);

	auto mark_buffer = [&](const lm::String& resource_name, bool read, bool write) {
		if (rg::is_buffer_registered(resource_name)) {
			BufferStatus& status = shader.buffer_status_map.get_or_create(resource_name)->value;
			status.read |= read;
			status.write |= write;
		}
	};

	auto access_helper = [&](u32 pointer_id, bool read, bool write) {
		assert(pointer_id < id_bound);
		auto mark_binding = [&](u32 binding) {
			BindingStatus& status = shader.resource_binding_map.get_or_create(binding)->value;
			status.read |= read;
			status.write |= write;
		};

		if (access_chains[pointer_id].valid) {
			const AccessChain& access_chain = access_chains[pointer_id];
			if (variables[access_chain.base_ptr_id].valid) {
				// Access chain starts from a declared variable.
				const u32 storage_class = variables[access_chain.base_ptr_id].storage_class;
				if (is_bound_buffer(storage_class)) {
					mark_binding(
						spvc_compiler_get_decoration(compiler, access_chain.base_ptr_id, SpvDecorationBinding));
				} else if (is_buffer(storage_class)) {
					// Physical buffer access through a previously loaded pointer.
					const u32 load_id = load_ids[access_chain.base_ptr_id];
					if (load_id && !buffer_pointer_names[load_id].empty()) {
						mark_buffer(buffer_pointer_names[load_id], read, write);
					}
				}
			} else {
				// Access chain starts from another access-chain/load result.
				const u32 load_id = load_ids[access_chain.base_ptr_id];
				if (load_id && !buffer_pointer_names[load_id].empty()) {
					mark_buffer(buffer_pointer_names[load_id], read, write);
				}
			}
		}
		if (!buffer_pointer_names[pointer_id].empty()) {
			mark_buffer(buffer_pointer_names[pointer_id], read, write);
		}
		// Theoretical case where the destination in OpStore is already a
		// declared pointer variable. Current shaderc/glslang output does not
		// normally emit this, but treating it as a bound resource is correct.
		if (variables[pointer_id].valid && is_bound_buffer(variables[pointer_id].storage_class)) {
			mark_binding(spvc_compiler_get_decoration(compiler, pointer_id, SpvDecorationBinding));
		}
	};

	// TODO: Support bindless image pointers in addition to buffer pointers.
	const u32* instruction = code + 5;
	while (instruction != code + code_size) {
		const u16 opcode = u16(instruction[0]);
		const u16 word_count = u16(instruction[0] >> 16);
		assert(word_count && instruction + word_count <= code + code_size);

		switch (opcode) {
			case SpvOpTypePointer:
				assert(word_count == 4);
				pointer_types[instruction[1]] = {.pointee_type_id = instruction[3], .valid = true};
				break;
			case SpvOpConstant:
				constant_ids[instruction[2]] = true;
				break;
			case SpvOpVariable: {
				assert(word_count >= 4);
				const u32 storage_class = instruction[3];
				if (storage_class != SpvStorageClassInput) {
					variables[instruction[2]] = {
						.storage_class = storage_class, .type_id = instruction[1], .valid = true};
				}
			} break;
			case SpvOpAccessChain: {
				assert(word_count >= 5);
				access_chains[instruction[2]] = {
					.base_ptr_id = instruction[3], .offset_idx = instruction[word_count - 1], .valid = true};
			} break;
			case SpvOpConvertUToPtr: {
				// Assumption: OpConvertUToPtr comes with OpAccessChain through OpLoad
				// instruction
				assert(word_count == 4);
				assert(access_chains[instruction[3]].valid);
				access_chains[instruction[2]] = access_chains[instruction[3]];
				access_chains[instruction[3]] = {};
			} break;
			case SpvOpLoad: {
				assert(word_count >= 4);
				const u32 pointer_id = instruction[3];
				const spvc_type result_type = spvc_compiler_get_type_handle(compiler, instruction[1]);
				assert(result_type);
				if (spvc_type_get_basetype(result_type) == SPVC_BASETYPE_UINT64) {
					// We are loading a pointer, update register map
					// Previous assumption also holds
					if (access_chains[pointer_id].valid) {
						const AccessChain access_chain = access_chains[pointer_id];
						if (variables[access_chain.base_ptr_id].valid &&
							is_bound_buffer(variables[access_chain.base_ptr_id].storage_class)) {
							const u32 binding = spvc_compiler_get_decoration(
								compiler, access_chain.base_ptr_id, SpvDecorationBinding);
							shader.resource_binding_map.get_or_create(binding)->value.read = true;
						}
						access_chains[instruction[2]] = access_chain;
						access_chains[pointer_id] = {};
					}
				} else if (pointer_types[instruction[1]].valid) {
					// Loading a pointer value. Record its source pointer ID.
					load_ids[instruction[2]] = pointer_id;
				} else if (access_chains[pointer_id].valid) {
					// Dereferencing a pointer. Resolve either a bound buffer or
					// a registered physical buffer pointer.
					const AccessChain& access_chain = access_chains[pointer_id];
					if (variables[access_chain.base_ptr_id].valid) {
						if (is_bound_buffer(variables[access_chain.base_ptr_id].storage_class)) {
							const u32 binding = spvc_compiler_get_decoration(
								compiler, access_chain.base_ptr_id, SpvDecorationBinding);
							shader.resource_binding_map.get_or_create(binding)->value.read = true;
						}
					} else {
						const u32 load_id = load_ids[access_chain.base_ptr_id];
						if (load_id && !buffer_pointer_names[load_id].empty()) {
							// TODO: Distinguish buffer and image pointers when
							// bindless image inference is added.
							mark_buffer(buffer_pointer_names[load_id], true, false);
						}
					}
				}

				if (!buffer_pointer_names[pointer_id].empty()) {
					mark_buffer(buffer_pointer_names[pointer_id], true, false);
				}
				if (variables[pointer_id].valid && is_bound_buffer(variables[pointer_id].storage_class)) {
					const u32 binding = spvc_compiler_get_decoration(compiler, pointer_id, SpvDecorationBinding);
					shader.resource_binding_map.get_or_create(binding)->value.read = true;
				}
			} break;
			case SpvOpAtomicIIncrement:
			case SpvOpAtomicIDecrement:
			case SpvOpAtomicISub:
			case SpvOpAtomicSMin:
			case SpvOpAtomicUMin:
			case SpvOpAtomicSMax:
			case SpvOpAtomicUMax:
			case SpvOpAtomicAnd:
			case SpvOpAtomicOr:
			case SpvOpAtomicXor:
			case SpvOpAtomicExchange:
			case SpvOpAtomicCompareExchange:
			case SpvOpAtomicCompareExchangeWeak:
			case SpvOpAtomicFlagTestAndSet:
			case SpvOpAtomicIAdd:
				access_helper(instruction[3], true, true);
				break;
			case SpvOpAtomicLoad:
				access_helper(instruction[3], true, false);
				break;
			case SpvOpAtomicStore:
				access_helper(instruction[1], false, true);
				break;
			case SpvOpAtomicFlagClear:
				access_helper(instruction[1], true, true);
				break;
			case SpvOpStore: {
				assert(word_count >= 3);
				access_helper(instruction[1], false, true);
				// A pointer stored for the first time identifies the named
				// registered buffer pointer used by later loads/access chains.
				if (access_chains[instruction[2]].valid) {
					const AccessChain& access_chain = access_chains[instruction[2]];
					const u32 pointer_id = instruction[1];
					if (constant_ids[access_chain.offset_idx]) {
						assert(variables[access_chain.base_ptr_id].valid);
						const u32 variable_type_id = variables[access_chain.base_ptr_id].type_id;
						assert(pointer_types[variable_type_id].valid);
						const spvc_type structure_type =
							spvc_compiler_get_type_handle(compiler, pointer_types[variable_type_id].pointee_type_id);
						assert(structure_type);
						const u32 member_count = spvc_type_get_num_member_types(structure_type);
						assert(member_count);

						lm::String container_name;
						lm::String pointer_name;
						const spvc_constant constant =
							spvc_compiler_get_constant_handle(compiler, access_chain.offset_idx);
						assert(constant);
						const u32 member_index = spvc_constant_get_scalar_u32(constant, 0, 0);
						for (u32 i = 0; i < member_count; i++) {
							const u32 member_type_id = spvc_type_get_member_type(structure_type, i);
							container_name = lm::str_from_cstr(spvc_compiler_get_name(compiler, member_type_id));
							pointer_name =
								lm::str_from_cstr(spvc_compiler_get_member_name(compiler, member_type_id, member_index));
						}
						const lm::String prefix = lm::str_concat(scratch_arena, container_name, "_");
						buffer_pointer_names[pointer_id] = lm::str_concat(scratch_arena, prefix, pointer_name);
					}
				}
			} break;
		}
		instruction += word_count;
	}
	return true;
}

static void reset_reflection(Shader& shader, lm::Arena* arena) {
	shader.stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	memset(shader.descriptor_types, 0, sizeof(shader.descriptor_types));
	shader.binding_mask = 0;
	shader.local_size_x = 1;
	shader.local_size_y = 1;
	shader.local_size_z = 1;
	shader.uses_push_constants = false;
	shader.push_constant_size = 0;
	shader.num_as_bindings = 0;
	shader.vertex_inputs.clear();
	shader.buffer_status_map = lm::hash_map_create<lm::String, BufferStatus>(arena, 128);
	shader.resource_binding_map = lm::hash_map_create<u32, BindingStatus>(arena, 128);
}

static bool parse_shader(Shader& shader, const u32* code, u64 code_size, lm::RenderPass* pass, lm::Arena* arena) {
	spvc_context context = nullptr;
	spvc_compiler compiler = nullptr;
	if (!create_spvc_compiler(code, code_size, context, compiler)) {
		return false;
	}

	spvc_resources resources = nullptr;
	if (!spvc_check(context, spvc_compiler_create_shader_resources(compiler, &resources), "resource reflection")) {
		return false;
	}

	auto reflect = [&](spvc_resource_type resource_type, VkDescriptorType descriptor_type) {
		SpvcResourceList list;
		if (!get_spvc_resources(context, resources, resource_type, list)) {
			return false;
		}
		for (size_t i = 0; i < list.size; i++) {
			const u32 binding = spvc_compiler_get_decoration(compiler, list.data[i].id, SpvDecorationBinding);
			LUMEN_ASSERT(binding < ARRAY_SIZE(shader.descriptor_types), "Shader descriptor binding is out of range");
			shader.binding_mask |= 1 << binding;
			shader.descriptor_types[binding] = descriptor_type;
		}
		return true;
	};

	auto max = [](u32 a, u32 b) { return a > b ? a : b; };
	shader.stage = get_shader_stage(spvc_compiler_get_execution_model(compiler));
	shader.local_size_x =
		max(1u, spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 0));
	shader.local_size_y =
		max(1u, spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 1));
	shader.local_size_z =
		max(1u, spvc_compiler_get_execution_mode_argument_by_index(compiler, SpvExecutionModeLocalSize, 2));

	if (!reflect(SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
		!reflect(SPVC_RESOURCE_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) ||
		!reflect(SPVC_RESOURCE_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
		!reflect(SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
		return false;
	}

	SpvcResourceList list;
	if (!get_spvc_resources(context, resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, list)) {
		return false;
	}
	if (shader.stage == VK_SHADER_STAGE_VERTEX_BIT) {
		for (size_t i = 0; i < list.size; i++) {
			const spvc_type type = spvc_compiler_get_type_handle(compiler, list.data[i].type_id);
			const spvc_type base_type = spvc_compiler_get_type_handle(compiler, list.data[i].base_type_id);
			VertexInput input;
			if (get_vertex_input(spvc_type_get_basetype(base_type), spvc_type_get_vector_size(type), input)) {
				shader.vertex_inputs.push_back(input);
			}
		}
	}

	if (!get_spvc_resources(context, resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, list)) {
		return false;
	}
	LUMEN_ASSERT(list.size <= 1, "Only 1 push constant is supported per shader at the moment!");
	shader.uses_push_constants = list.size != 0;
	if (list.size) {
		const spvc_type type = spvc_compiler_get_type_handle(compiler, list.data[0].base_type_id);
		size_t push_constant_size = 0;
		if (!spvc_check(context, spvc_compiler_get_declared_struct_size(compiler, type, &push_constant_size),
						"push constant size reflection")) {
			return false;
		}
		LUMEN_ASSERT(push_constant_size <= U32_MAX, "Push constant block exceeds u32 size");
		shader.push_constant_size = (u32)push_constant_size;
	}

	if (rg::settings().shader_inference) {
		{
			lm::ScratchArena scratch(arena);
			if (!parse_spirv(context, compiler, shader, code, code_size, pass, scratch.arena)) {
				return false;
			}
		}

		// We ultimately use the string inside buffer_status_map in the render graph
		// Therefore we need to make sure the string is persisted in the shader arena
		for (auto& entry : shader.buffer_status_map) {
			const lm::String popped_key = entry.key;
			const lm::String persistent_key = lm::str_dup(arena, popped_key);
			assert(persistent_key.data + persistent_key.size <= popped_key.data);
			entry.key = persistent_key;
		}
	}
	return true;
}

Shader::Shader(const lm::String& filename) : filename(filename) {}

i32 Shader::compile(lm::RenderPass* pass) {
	assert(!name_with_macros.empty() && name_with_macros.is_cstr());
	LUMEN_TRACE("Compiling shader: %s", name_with_macros.data);

	lm::Arena* arena = get_shader_arena();
	shaderc_shader_kind kind;
	if (!get_shader_kind(filename, kind)) {
		lm::log(lm::LOG_ERROR, "Unsupported shader extension: %s", filename.data);
		return -1;
	}

	shaderc_compilation_result_t result = nullptr;
	{
		lm::ScratchArena scratch(arena);
		lm::String source;
		if (!read_file(scratch.arena, filename, source)) {
			lm::log(lm::LOG_ERROR, "Failed to open shader file: %s", filename.data);
			return -1;
		}
		result = compile_file(filename, kind, source, pass, scratch.arena);
	}

	if (!result) {
		lm::log(lm::LOG_ERROR, "Failed to initialize shaderc for: %s", filename.data);
		return -1;
	}
	if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
		lm::log(lm::LOG_ERROR, "%s", shaderc_result_get_error_message(result));
		shaderc_result_release(result);
		return -1;
	}

	const u64 binary_bytes = shaderc_result_get_length(result);
	if (!binary_bytes || binary_bytes % sizeof(u32)) {
		lm::log(lm::LOG_ERROR, "Shaderc returned invalid SPIR-V for: %s", filename.data);
		shaderc_result_release(result);
		return -1;
	}

	const u64 word_count = binary_bytes / sizeof(u32);
	binary = lm::fixed_array_create<u32>(arena, word_count);
	memcpy(binary.data, shaderc_result_get_bytes(result), binary_bytes);
	binary.size = word_count;
	shaderc_result_release(result);

	// Clear shader data in case this is a recompile
	reset_reflection(*this, arena);
	if (!parse_shader(*this, binary.data, binary.size, pass, arena)) {
		return -1;
	}
	return 0;
}

VkShaderModule Shader::create_vk_shader_module(const VkDevice& device) const {
	VkShaderModuleCreateInfo shader_module_CI{};
	shader_module_CI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_CI.codeSize = binary.size * sizeof(u32);
	shader_module_CI.pCode = binary.data;

	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &shader_module_CI, nullptr, &shader_module) != VK_SUCCESS) {
		LUMEN_ERROR("Failed to create shader module!");
	}
	return shader_module;
}

void shader_arena_reset() {
	os::ScopedLock lock(_shader_worker_arenas_mutex);
	for (lm::Arena* arena : _shader_worker_arenas) {
		arena->clear();
	}
}
}  // namespace vk
