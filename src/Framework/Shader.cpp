#include "Shader.h"
#include "RenderGraph.h"
#include <spirv_cross/spirv.h>
#include <spirv_cross/spirv_cross.hpp>
#include <shaderc/shaderc.h>

constexpr u64 MAX_SHADER_WORKER_ARENAS = 64;

struct ShaderThreadState {
	lm::Arena* arena = nullptr;
	// Reused for the worker's lifetime process shutdown reclaims it.
	shaderc_compiler_t compiler = nullptr;
};

struct ShaderIncludeContext {
	lm::Arena* arena;
};

thread_local ShaderThreadState _shader_thread_state;

// We need this to be able to reset all threads' arenas
static lm::SmallArray<lm::Arena*, MAX_SHADER_WORKER_ARENAS> _shader_worker_arenas;
static std::mutex _shader_worker_arenas_mutex;

static u64 string_content_size(const lm::String& string) {
	return string.size && string.data[string.size - 1] == '\0' ? string.size - 1 : string.size;
}

static lm::String string_content(const lm::String& string) { return {string.data, string_content_size(string)}; }

static lm::Arena* get_shader_arena() {
	if (!_shader_thread_state.arena) {
		std::lock_guard<std::mutex> lock(_shader_worker_arenas_mutex);
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
	auto* result = (shaderc_include_result*)arena->allocate(sizeof(shaderc_include_result));
	result->source_name = "";
	result->source_name_length = 0;
	result->content = message;
	result->content_length = strlen(message);
	return result;
}

static shaderc_include_result* shader_include_resolve(void* user_data, const char* requested_source, int include_type,
													  const char* requesting_source, size_t) {
	auto* context = (ShaderIncludeContext*)user_data;
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

	auto* result = (shaderc_include_result*)arena->allocate(sizeof(shaderc_include_result));
	result->source_name = resolved_path.data;
	result->source_name_length = string_content_size(resolved_path);
	result->content = contents.data;
	result->content_length = contents.size;
	return result;
}

static void shader_include_release(void*, shaderc_include_result*) {}

static void add_macros(const vk::ShaderMacroArray& macros, shaderc_compile_options_t options, lm::Arena* arena) {
	for (const auto& macro : macros) {
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
	add_macros(pass->rg->global_macro_defines, options, scratch_arena);
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
	static const StageExtension stages[] = {
		{"vert", shaderc_vertex_shader},	   {"frag", shaderc_fragment_shader},
		{"comp", shaderc_compute_shader},	   {"geom", shaderc_geometry_shader},
		{"tesc", shaderc_tess_control_shader}, {"tese", shaderc_tess_evaluation_shader},
		{"rgen", shaderc_raygen_shader},	   {"rahit", shaderc_anyhit_shader},
		{"rchit", shaderc_closesthit_shader},  {"rmiss", shaderc_miss_shader},
		{"rint", shaderc_intersection_shader}, {"rcall", shaderc_callable_shader},
		{"task", shaderc_task_shader},		   {"mesh", shaderc_mesh_shader},
	};
	for (const StageExtension& stage : stages) {
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
static bool get_vertex_input(spirv_cross::SPIRType::BaseType base_type, u32 vector_size, VertexInput& input) {
	struct VertexInputMapping {
		spirv_cross::SPIRType::BaseType base_type;
		u32 vector_size;
		VertexInput input;
	};
	static const VertexInputMapping mappings[] = {
		{spirv_cross::SPIRType::BaseType::Int, 1u, {VK_FORMAT_R32_SINT, (u32)sizeof(i32)}},
		{spirv_cross::SPIRType::BaseType::Int, 2u, {VK_FORMAT_R32G32_SINT, 2 * (u32)sizeof(i32)}},
		{spirv_cross::SPIRType::BaseType::Int, 3u, {VK_FORMAT_R32G32B32_SINT, 3 * (u32)sizeof(i32)}},
		{spirv_cross::SPIRType::BaseType::Int, 4u, {VK_FORMAT_R32G32B32A32_SINT, 4 * (u32)sizeof(i32)}},
		{spirv_cross::SPIRType::BaseType::UInt, 1u, {VK_FORMAT_R32_UINT, (u32)sizeof(u32)}},
		{spirv_cross::SPIRType::BaseType::UInt, 2u, {VK_FORMAT_R32G32_UINT, 2 * (u32)sizeof(u32)}},
		{spirv_cross::SPIRType::BaseType::UInt, 3u, {VK_FORMAT_R32G32B32_UINT, 3 * (u32)sizeof(u32)}},
		{spirv_cross::SPIRType::BaseType::UInt, 4u, {VK_FORMAT_R32G32B32A32_UINT, 4 * (u32)sizeof(u32)}},
		{spirv_cross::SPIRType::BaseType::Short, 1u, {VK_FORMAT_R16_SINT, (u32)sizeof(i16)}},
		{spirv_cross::SPIRType::BaseType::Short, 2u, {VK_FORMAT_R16G16_SINT, 2 * (u32)sizeof(i16)}},
		{spirv_cross::SPIRType::BaseType::Short, 3u, {VK_FORMAT_R16G16B16_SINT, 3 * (u32)sizeof(i16)}},
		{spirv_cross::SPIRType::BaseType::Short, 4u, {VK_FORMAT_R16G16B16A16_SINT, 4 * (u32)sizeof(i16)}},
		{spirv_cross::SPIRType::BaseType::UShort, 1u, {VK_FORMAT_R16_UINT, (u32)sizeof(u16)}},
		{spirv_cross::SPIRType::BaseType::UShort, 2u, {VK_FORMAT_R16G16_UINT, 2 * (u32)sizeof(u16)}},
		{spirv_cross::SPIRType::BaseType::UShort, 3u, {VK_FORMAT_R16G16B16_UINT, 3 * (u32)sizeof(u16)}},
		{spirv_cross::SPIRType::BaseType::UShort, 4u, {VK_FORMAT_R16G16B16A16_UINT, 4 * (u32)sizeof(u16)}},
		{spirv_cross::SPIRType::BaseType::Float, 1u, {VK_FORMAT_R32_SFLOAT, (u32)sizeof(f32)}},
		{spirv_cross::SPIRType::BaseType::Float, 2u, {VK_FORMAT_R32G32_SFLOAT, 2 * (u32)sizeof(f32)}},
		{spirv_cross::SPIRType::BaseType::Float, 3u, {VK_FORMAT_R32G32B32_SFLOAT, 3 * (u32)sizeof(f32)}},
		{spirv_cross::SPIRType::BaseType::Float, 4u, {VK_FORMAT_R32G32B32A32_SFLOAT, 4 * (u32)sizeof(f32)}},
		{spirv_cross::SPIRType::BaseType::Half, 1u, {VK_FORMAT_R16_SFLOAT, (u32)sizeof(u16)}},
		{spirv_cross::SPIRType::BaseType::Half, 2u, {VK_FORMAT_R16G16_SFLOAT, 2 * (u32)sizeof(u16)}},
		{spirv_cross::SPIRType::BaseType::Half, 3u, {VK_FORMAT_R16G16B16_SFLOAT, 3 * (u32)sizeof(u16)}},
		{spirv_cross::SPIRType::BaseType::Half, 4u, {VK_FORMAT_R16G16B16A16_SFLOAT, 4 * (u32)sizeof(u16)}},
	};

	for (const VertexInputMapping& mapping : mappings) {
		if (mapping.base_type == base_type && mapping.vector_size == vector_size) {
			input = mapping.input;
			return true;
		}
	}
	return false;
}

static VkShaderStageFlagBits get_shader_stage(spv::ExecutionModel execution_model) {
	switch (execution_model) {
		case spv::ExecutionModelVertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case spv::ExecutionModelFragment:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case spv::ExecutionModelGLCompute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
		case spv::ExecutionModelTaskNV:
			return VK_SHADER_STAGE_TASK_BIT_NV;
		case spv::ExecutionModelMeshNV:
			return VK_SHADER_STAGE_MESH_BIT_NV;
		case spv::ExecutionModelRayGenerationKHR:
			return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case spv::ExecutionModelIntersectionKHR:
			return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case spv::ExecutionModelAnyHitKHR:
			return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case spv::ExecutionModelClosestHitKHR:
			return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case spv::ExecutionModelMissKHR:
			return VK_SHADER_STAGE_MISS_BIT_KHR;
		case spv::ExecutionModelCallableKHR:
			return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		default:
			assert(!"Unsupported execution model");
			return VkShaderStageFlagBits(0);
	}
}

static u32 get_pc_size(spirv_cross::Compiler& compiler, const spirv_cross::SPIRType& type) {
	u32 pc_size = 0;
	for (auto member_type_id : type.member_types) {
		const auto& member_type = compiler.get_type(member_type_id);
		if (member_type.basetype == spirv_cross::SPIRType::BaseType::Struct) {
			pc_size += get_pc_size(compiler, member_type);
			continue;
		}

		const u32 element_count = member_type.columns * member_type.vecsize;
		switch (member_type.basetype) {
			case spirv_cross::SPIRType::BaseType::SByte:
			case spirv_cross::SPIRType::BaseType::UByte:
				pc_size += element_count;
				break;
			case spirv_cross::SPIRType::BaseType::Short:
			case spirv_cross::SPIRType::BaseType::UShort:
			case spirv_cross::SPIRType::BaseType::Half:
				pc_size += element_count * 2;
				break;
			case spirv_cross::SPIRType::BaseType::Int:
			case spirv_cross::SPIRType::BaseType::UInt:
			case spirv_cross::SPIRType::BaseType::Float:
				pc_size += element_count * 4;
				break;
			case spirv_cross::SPIRType::BaseType::Double:
			case spirv_cross::SPIRType::BaseType::Int64:
			case spirv_cross::SPIRType::BaseType::UInt64:
				pc_size += element_count * 8;
				break;
			default:
				LUMEN_ERROR("Unexpected push constant type!");
		}
	}
	return pc_size;
}

static bool is_bound_buffer(u32 storage_class) { return storage_class == spv::StorageClassStorageBuffer; }

static bool is_buffer(u32 storage_class) {
	return storage_class == spv::StorageClassStorageBuffer || storage_class == spv::StorageClassPhysicalStorageBuffer;
}

static void parse_spirv(spirv_cross::Compiler& compiler, Shader& shader, const u32* code, u64 code_size,
						lm::RenderPass* pass, lm::Arena* scratch_arena) {
	// Update active image and buffer status before walking individual instructions:
	// storage images can be read and/or written, sampled images are reads, and
	// active storage buffers must remain visible even if no access is inferred below.
	const auto active_variables = compiler.get_active_interface_variables();
	const auto active_resources = compiler.get_shader_resources(active_variables);
	for (const auto& sampled_image : active_resources.sampled_images) {
		const u32 binding = compiler.get_decoration(sampled_image.id, spv::DecorationBinding);
		shader.resource_binding_map.insert(binding, BindingStatus{.read = true, .active = true});
	}
	for (const auto& storage_image : active_resources.storage_images) {
		const u32 binding = compiler.get_decoration(storage_image.id, spv::DecorationBinding);
		BindingStatus& status = shader.resource_binding_map.get_or_create(binding)->value;
		status.read |= !compiler.has_decoration(storage_image.id, spv::DecorationNonReadable);
		status.write |= !compiler.has_decoration(storage_image.id, spv::DecorationNonWritable);
		status.active = true;
	}
	for (const auto& storage_buffer : active_resources.storage_buffers) {
		const u32 binding = compiler.get_decoration(storage_buffer.id, spv::DecorationBinding);
		shader.resource_binding_map.get_or_create(binding)->value.active = true;
	}
	for (const auto& acceleration_structure : active_resources.acceleration_structures) {
		const u32 set = compiler.get_decoration(acceleration_structure.id, spv::DecorationDescriptorSet);
		LUMEN_ASSERT(set == 1, "Acceleration structure must be in descriptor set 1");
		shader.num_as_bindings++;
	}

	assert(code[0] == SpvMagicNumber);
	const u32 id_bound = code[3];
	struct Variable {
		u32 storage_class = 0;
		bool valid = false;
	};
	struct AccessChain {
		u32 base_ptr_id = 0;
		u32 offset_idx = 0;
		bool valid = false;
	};

	auto access_chains = id_array_create<AccessChain>(scratch_arena, id_bound);
	auto variables = id_array_create<Variable>(scratch_arena, id_bound);
	auto load_ids = id_array_create<u32>(scratch_arena, id_bound);	// Result ID -> pointer ID
	auto constant_ids = id_array_create<bool>(scratch_arena, id_bound);
	// Pointer ID -> registered buffer pointer name.
	auto buffer_pointer_names = id_array_create<lm::String>(scratch_arena, id_bound);

	auto mark_buffer = [&](const lm::String& resource_name, bool read, bool write) {
		auto entry = pass->rg->registered_buffer_pointers.find(resource_name);
		if (entry) {
			BufferStatus& status = shader.buffer_status_map.get_or_create(entry->key)->value;
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
					mark_binding(compiler.get_decoration(access_chain.base_ptr_id, spv::DecorationBinding));
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
			mark_binding(compiler.get_decoration(pointer_id, spv::DecorationBinding));
		}
	};

	// TODO: Support bindless image pointers in addition to buffer pointers.
	const u32* instruction = code + 5;
	while (instruction != code + code_size) {
		const u16 opcode = u16(instruction[0]);
		const u16 word_count = u16(instruction[0] >> 16);
		assert(word_count && instruction + word_count <= code + code_size);

		switch (opcode) {
			case SpvOpConstant:
				constant_ids[instruction[2]] = true;
				break;
			case SpvOpVariable: {
				assert(word_count >= 4);
				const u32 storage_class = instruction[3];
				if (storage_class != spv::StorageClassInput) {
					variables[instruction[2]] = {.storage_class = storage_class, .valid = true};
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
				const auto& result_type = compiler.get_type(instruction[1]);
				if (result_type.basetype == spirv_cross::SPIRType::UInt64) {
					// We are loading a pointer, update register map
					// Previous assumption also holds
					if (access_chains[pointer_id].valid) {
						const AccessChain access_chain = access_chains[pointer_id];
						if (is_bound_buffer(compiler.get_storage_class(access_chain.base_ptr_id))) {
							const u32 binding =
								compiler.get_decoration(access_chain.base_ptr_id, spv::DecorationBinding);
							shader.resource_binding_map.get_or_create(binding)->value.read = true;
						}
						access_chains[instruction[2]] = access_chain;
						access_chains[pointer_id] = {};
					}
				} else if (result_type.pointer) {
					// Loading a pointer value. Record its source pointer ID.
					load_ids[instruction[2]] = pointer_id;
				} else if (access_chains[pointer_id].valid) {
					// Dereferencing a pointer. Resolve either a bound buffer or
					// a registered physical buffer pointer.
					const AccessChain& access_chain = access_chains[pointer_id];
					if (variables[access_chain.base_ptr_id].valid) {
						if (is_bound_buffer(variables[access_chain.base_ptr_id].storage_class)) {
							const u32 binding =
								compiler.get_decoration(access_chain.base_ptr_id, spv::DecorationBinding);
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
					const u32 binding = compiler.get_decoration(pointer_id, spv::DecorationBinding);
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
						const auto& pointer_type = compiler.get_type_from_variable(access_chain.base_ptr_id);
						const auto& structure_type = compiler.get_type(pointer_type.parent_type);
						assert(!structure_type.member_types.empty());

						lm::String container_name;
						lm::String pointer_name;
						const u32 member_index = compiler.get_constant(access_chain.offset_idx).scalar();
						for (auto member_type_id : structure_type.member_types) {
							const auto& container = compiler.get_name(member_type_id);
							const auto& member = compiler.get_member_name(member_type_id, member_index);
							container_name = {(char*)container.data(), (u64)container.size()};
							pointer_name = {(char*)member.data(), (u64)member.size()};
						}
						const lm::String prefix = lm::str_concat(scratch_arena, container_name, "_");
						buffer_pointer_names[pointer_id] = lm::str_concat(scratch_arena, prefix, pointer_name);
					}
				}
			} break;
		}
		instruction += word_count;
	}
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

static void parse_shader(Shader& shader, const u32* code, u64 code_size, lm::RenderPass* pass, lm::Arena* arena) {
	spirv_cross::Compiler compiler(code, code_size);
	const auto resources = compiler.get_shader_resources();

	auto reflect = [&shader, &compiler](const spirv_cross::Resource& resource, VkDescriptorType type) {
		const u32 binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		LUMEN_ASSERT(binding < ARRAY_SIZE(shader.descriptor_types), "Shader descriptor binding is out of range");
		shader.binding_mask |= 1 << binding;
		shader.descriptor_types[binding] = type;
	};

	auto max = [](u32 a, u32 b) { return a > b ? a : b; };
	shader.stage = get_shader_stage(compiler.get_execution_model());
	shader.local_size_x = max(1u, compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0));
	shader.local_size_y = max(1u, compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1));
	shader.local_size_z = max(1u, compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2));

	shader.uses_push_constants = !resources.push_constant_buffers.empty();
	for (const auto& resource : resources.uniform_buffers) {
		reflect(resource, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}
	for (const auto& resource : resources.storage_buffers) {
		reflect(resource, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	}
	for (const auto& resource : resources.storage_images) {
		reflect(resource, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	}
	for (const auto& resource : resources.sampled_images) {
		reflect(resource, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}

	if (shader.stage == VK_SHADER_STAGE_VERTEX_BIT) {
		for (const auto& resource : resources.stage_inputs) {
			const auto& type = compiler.get_type(resource.type_id);
			const auto& base_type = compiler.get_type(resource.base_type_id);
			VertexInput input;
			if (get_vertex_input(base_type.basetype, type.vecsize, input)) {
				shader.vertex_inputs.push_back(input);
			}
		}
	}

	LUMEN_ASSERT(resources.push_constant_buffers.size() <= 1,
				 "Only 1 push constant is supported per shader at the moment!");
	if (!resources.push_constant_buffers.empty()) {
		const auto& type = compiler.get_type(resources.push_constant_buffers[0].type_id);
		shader.push_constant_size = get_pc_size(compiler, type);
	}
	if (pass->rg->settings.shader_inference) {
		lm::ScratchArena scratch(arena);
		parse_spirv(compiler, shader, code, code_size, pass, scratch.arena);
	}
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
	parse_shader(*this, binary.data, binary.size, pass, arena);
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
	std::lock_guard<std::mutex> lock(_shader_worker_arenas_mutex);
	for (lm::Arena* arena : _shader_worker_arenas) {
		arena->clear();
	}
}
}  // namespace vk
