#include "../LumenPCH.h"
#include "Shader.h"
#include "RenderGraph.h"
#include <spirv_cross/spirv.h>
#include <spirv_cross/spirv_glsl.hpp>

#if USE_SHADERC
#include <shaderc/shaderc.hpp>
#include <glslc/file_includer.h>
#include <libshaderc_util/file_finder.h>
#endif	//  USE_SHADERC

namespace vk {
enum class ResourceType { UniformBuffer, StorageBuffer, StorageImage, SampledImage, AccelarationStructure };

static std::unordered_map<ResourceType, VkDescriptorType> descriptor_Type_map = {
	{ResourceType::UniformBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
	{ResourceType::StorageBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
	{ResourceType::StorageImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
	{ResourceType::SampledImage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
	{ResourceType::AccelarationStructure, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
};

struct input_map_hash {
	template <class T1, class T2>
	uint64_t operator()(const std::pair<T1, T2>& p) const {
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);
		return h1 << 16 | h2;
	}
};

static std::unordered_map<std::pair<spirv_cross::SPIRType::BaseType, uint32_t>, std::pair<VkFormat, uint32_t>,
						  input_map_hash>
	vertex_input_map = {
		{{spirv_cross::SPIRType::BaseType::Int, 1u}, {VK_FORMAT_R32_SINT, (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::Int, 2u}, {VK_FORMAT_R32G32_SINT, 2 * (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::Int, 3u}, {VK_FORMAT_R32G32B32_SINT, 3 * (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::Int, 4u}, {VK_FORMAT_R32G32B32A32_SINT, 4 * (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::Int, 1u}, {VK_FORMAT_R32_UINT, (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::UInt, 2u}, {VK_FORMAT_R32G32_UINT, 2 * (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::UInt, 3u}, {VK_FORMAT_R32G32B32_UINT, 3 * (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::UInt, 4u}, {VK_FORMAT_R32G32B32A32_UINT, 4 * (uint32_t)sizeof(int)}},
		{{spirv_cross::SPIRType::BaseType::Short, 1u}, {VK_FORMAT_R16_SINT, (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::Short, 2u}, {VK_FORMAT_R16G16_SINT, 2 * (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::Short, 3u}, {VK_FORMAT_R16G16B16_SINT, 3 * (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::Short, 4u}, {VK_FORMAT_R16G16B16A16_SINT, 4 * (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::UShort, 1u}, {VK_FORMAT_R16_UINT, (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::UShort, 2u}, {VK_FORMAT_R16G16_UINT, 2 * (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::UShort, 3u}, {VK_FORMAT_R16G16B16_UINT, 3 * (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::UShort, 4u}, {VK_FORMAT_R16G16B16A16_UINT, 4 * (uint32_t)sizeof(int) / 2}},
		{{spirv_cross::SPIRType::BaseType::Float, 1u}, {VK_FORMAT_R32_SFLOAT, (uint32_t)sizeof(float)}},
		{{spirv_cross::SPIRType::BaseType::Float, 2u}, {VK_FORMAT_R32G32_SFLOAT, 2 * (uint32_t)sizeof(float)}},
		{{spirv_cross::SPIRType::BaseType::Float, 3u}, {VK_FORMAT_R32G32B32_SFLOAT, 3 * (uint32_t)sizeof(float)}},
		{{spirv_cross::SPIRType::BaseType::Float, 4u}, {VK_FORMAT_R32G32B32A32_SFLOAT, 4 * (uint32_t)sizeof(float)}},
		{{spirv_cross::SPIRType::BaseType::Half, 1u}, {VK_FORMAT_R16_SFLOAT, (uint32_t)sizeof(float) / 2}},
		{{spirv_cross::SPIRType::BaseType::Half, 2u}, {VK_FORMAT_R16G16_SFLOAT, 2 * (uint32_t)sizeof(float) / 2}},
		{{spirv_cross::SPIRType::BaseType::Half, 3u}, {VK_FORMAT_R16G16B16_SFLOAT, 3 * (uint32_t)sizeof(float) / 2}},
		{{spirv_cross::SPIRType::BaseType::Half, 4u}, {VK_FORMAT_R16G16B16A16_SFLOAT, 4 * (uint32_t)sizeof(float) / 2}},
};

static VkShaderStageFlagBits get_shader_stage(spv::ExecutionModel executionModel) {
	switch (executionModel) {
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
		default:
			assert(!"Unsupported execution model");
			return VkShaderStageFlagBits(0);
	}
}

static uint32_t get_pc_size(spirv_cross::CompilerGLSL& glsl, const spirv_cross::SPIRType& type) {
	uint32_t num_types = (uint32_t)type.member_types.size();
	uint32_t pc_size = 0;
	for (uint32_t i = 0; i < num_types; i++) {
		auto member_type_id = type.member_types[i];
		auto member_type_handle = glsl.get_type(member_type_id);
		auto member_base_type = member_type_handle.basetype;
		if (member_base_type != spirv_cross::SPIRType::BaseType::Struct) {
			auto vec_size = member_type_handle.vecsize;
			auto num_cols = member_type_handle.columns;
			switch (member_base_type) {
				case spirv_cross::SPIRType::BaseType::SByte:
				case spirv_cross::SPIRType::BaseType::UByte:
					pc_size += num_cols * vec_size * 1;
					break;
				case spirv_cross::SPIRType::BaseType::Short:
				case spirv_cross::SPIRType::BaseType::UShort:
				case spirv_cross::SPIRType::BaseType::Half:
					pc_size += num_cols * vec_size * 2;
					break;
				case spirv_cross::SPIRType::BaseType::Int:
				case spirv_cross::SPIRType::BaseType::UInt:
				case spirv_cross::SPIRType::BaseType::Float:
					pc_size += num_cols * vec_size * 4;
					break;
				case spirv_cross::SPIRType::BaseType::Double:
				case spirv_cross::SPIRType::BaseType::Int64:
				case spirv_cross::SPIRType::BaseType::UInt64:
					pc_size += num_cols * vec_size * 8;
					break;
				default:
					LUMEN_ERROR("Unexpected push constant type!");
			}
		} else {
			pc_size += get_pc_size(glsl, member_type_handle);
		}
	}
	return pc_size;
}

static bool is_bound_buffer(uint32_t storage_class) {
	if (storage_class == spv::StorageClassStorageBuffer) {
		return true;
	}
	return false;
}

static bool is_buffer(uint32_t storage_class) {
	if (storage_class == spv::StorageClassStorageBuffer || storage_class == spv::StorageClassPhysicalStorageBuffer) {
		return true;
	}
	return false;
}

static void parse_spirv(spirv_cross::CompilerGLSL& glsl, const spirv_cross::ShaderResources& resources, Shader& shader,
						const uint32_t* code, size_t code_size, lumen::RenderPass* pass) {
	// Update the resource status of image types
	// Storage Image -> Write
	// Sampled Image -> Read
	auto active_vars = glsl.get_active_interface_variables();
	auto active_resources = glsl.get_shader_resources(active_vars);
	for (auto& sampled_img : active_resources.sampled_images) {
		auto binding = glsl.get_decoration(sampled_img.id, spv::DecorationBinding);
		shader.resource_binding_map[binding].read = true;
		shader.resource_binding_map[binding].active = true;
	}
	for (auto& storage_img : active_resources.storage_images) {
		auto binding = glsl.get_decoration(storage_img.id, spv::DecorationBinding);
		shader.resource_binding_map[binding].write = true;
		shader.resource_binding_map[binding].active = true;
	}
	for (auto& storage_buffer : active_resources.storage_buffers) {
		auto binding = glsl.get_decoration(storage_buffer.id, spv::DecorationBinding);
		shader.resource_binding_map[binding].active = true;
	}
	assert(code[0] == SpvMagicNumber);

	// uint32_t num_ids = code[3];

	const uint32_t* insn = code + 5;

	struct Variable {
		uint32_t storage_class;
	};

	struct AccessChain {
		uint32_t base_ptr_id;
		uint32_t base_idx;
		uint32_t offset_idx;
	};

	std::unordered_map<uint32_t, AccessChain> access_chain_map;
	std::unordered_map<uint32_t, Variable> variable_map;
	std::unordered_map<uint32_t, uint32_t> load_map;		  // Dst Id - Ptr Id
	std::unordered_map<uint32_t, uint32_t> store_access_map;  //  Ptr Data from load_map
	std::unordered_map<uint32_t, uint32_t> constant_map;
	std::unordered_map<uint32_t, std::string> buffer_ptr_hash_map;

	auto store_helper = [&](uint32_t store_id) {
		if (access_chain_map.find(store_id) != access_chain_map.end()) {
			const auto& access_chain = access_chain_map[store_id];
			if (variable_map.find(access_chain.base_ptr_id) != variable_map.end()) {
				// Access chain has variable
				const auto variable_storage_class = variable_map[access_chain.base_ptr_id].storage_class;
				// TODO: Check if buffer
				if (is_bound_buffer(variable_storage_class)) {
					// Bound resource
					auto binding = glsl.get_decoration(access_chain.base_ptr_id, spv::DecorationBinding);
					shader.resource_binding_map[binding].write = true;
				} else if (is_buffer(variable_storage_class)) {
					// Via pointer
					auto ptr_var_id = load_map[access_chain.base_ptr_id];
					auto var_name = glsl.get_name(ptr_var_id);
					auto var_type = glsl.get_type_from_variable(ptr_var_id);
					assert(buffer_ptr_hash_map.find(ptr_var_id) != buffer_ptr_hash_map.end());
					const auto& res = buffer_ptr_hash_map[ptr_var_id];
					if (pass->rg->registered_buffer_pointers.find(res) != pass->rg->registered_buffer_pointers.end()) {
						shader.buffer_status_map[res].write = true;
					}
				}
			} else if (load_map.find(access_chain.base_ptr_id) != load_map.end()) {
				// Access chain has loads
				// If it has loads, it should be a buffer pointer
				const auto& res = buffer_ptr_hash_map[load_map[access_chain.base_ptr_id]];
				if (pass->rg->registered_buffer_pointers.find(res) != pass->rg->registered_buffer_pointers.end()) {
					shader.buffer_status_map[res].write = true;
				}
			}
		}
		// Theoretical case where _%a_ in _OpStore %a %b_ is already a
		// declared pointer variable In this case the resource should be
		// bound, as it implies 0 offset Fortunately, glslang or shaderc
		// don't do this as of SPIR-V 1.6
		if (variable_map.find(store_id) != variable_map.end()) {
			if (is_bound_buffer(variable_map[store_id].storage_class)) {
				auto binding = glsl.get_decoration(store_id, spv::DecorationBinding);
				shader.resource_binding_map[binding].write = true;
			}
		}
	};

	// TODO: Support for bindless images
	while (insn != code + code_size) {
		uint16_t opcode = uint16_t(insn[0]);
		uint16_t word_count = uint16_t(insn[0] >> 16);

		switch (opcode) {
			case SpvOpConstant: {
				constant_map[insn[2]] = 1;
			} break;

			case SpvOpVariable: {
				assert(word_count >= 4);
				uint32_t storage_class = insn[3];
				auto type = glsl.get_type_from_variable(insn[2]);
				if (storage_class != spv::StorageClassInput) {
					variable_map[insn[2]] = Variable{.storage_class = storage_class};
				}
			} break;
			case SpvOpAccessChain: {
				assert(word_count >= 4);
				uint32_t result_id = insn[2];
				uint32_t base_ptr_id = insn[3];
				auto base_idx = insn[4];
				auto idx = insn[5];
				access_chain_map[result_id] = {base_ptr_id, base_idx, idx};
			} break;
			case SpvOpConvertUToPtr: {
				// Assumption: OpConvertUToPtr comes with OpAccessChain through OpLoad
				// instruction
				assert(word_count == 4);
				assert(access_chain_map.find(insn[3]) != access_chain_map.end());
				auto nh = access_chain_map.extract(insn[3]);
				nh.key() = insn[2];
				access_chain_map.insert(std::move(nh));
			} break;

			case SpvOpLoad: {
				assert(word_count >= 3);
				uint32_t ptr_var_id = insn[3];
				auto result_type = glsl.get_type(insn[1]);

				if (result_type.basetype == spirv_cross::SPIRType::UInt64) {
					// We are loading a pointer, update register map
					// Previous assumption also holds
					uint32_t id = insn[3];
					if (access_chain_map.find(id) != access_chain_map.end()) {
						const AccessChain& access_chain = access_chain_map[id];
						auto storage_class = glsl.get_storage_class(access_chain.base_ptr_id);
						if (is_bound_buffer(storage_class)) {
							auto binding = glsl.get_decoration(access_chain.base_ptr_id, spv::DecorationBinding);
							shader.resource_binding_map[binding].read = true;
						}
						auto nh = access_chain_map.extract(id);
						nh.key() = insn[2];
						access_chain_map.insert(std::move(nh));
					}
				} else if (result_type.pointer) {
					// We are not loading a pointer but dereferencing it
					load_map[insn[2]] = insn[3];
				} else {
					// Result type is not a pointer, get binding
					if (access_chain_map.find(insn[3]) != access_chain_map.end()) {
						std::string container_name;
						std::string ptr_name;
						const AccessChain& access_chain = access_chain_map[insn[3]];

						if (variable_map.find(access_chain.base_ptr_id) != variable_map.end()) {
							// Load was made through a variable
							const auto variable_storage_class = variable_map[access_chain.base_ptr_id].storage_class;
							if (is_bound_buffer(variable_storage_class)) {
								auto binding = glsl.get_decoration(access_chain.base_ptr_id, spv::DecorationBinding);
								shader.resource_binding_map[binding].read = true;
							} else {
								// Variable + buffer pointer?
							}
						} else if (load_map.find(access_chain.base_ptr_id) != load_map.end()) {
							// Load was made through an access chain + load
							if (buffer_ptr_hash_map.find(load_map[access_chain.base_ptr_id]) !=
								buffer_ptr_hash_map.end()) {
								// TODO: Distinguish buffer and image pointers
								// when we add bindless images in the future
								const auto& res = buffer_ptr_hash_map[load_map[access_chain.base_ptr_id]];
								if (pass->rg->registered_buffer_pointers.find(res) !=
									pass->rg->registered_buffer_pointers.end()) {
									shader.buffer_status_map[res].read = true;
								}
							}
						}
					}
				}
				if (buffer_ptr_hash_map.find(ptr_var_id) != buffer_ptr_hash_map.end()) {
					// TODO: Distinguish buffer and image pointers when we add
					// bindless images in the future
					const auto& res = buffer_ptr_hash_map[ptr_var_id];
					if (pass->rg->registered_buffer_pointers.find(res) != pass->rg->registered_buffer_pointers.end()) {
						shader.buffer_status_map[res].read = true;
					}
				}

				if (variable_map.find(ptr_var_id) != variable_map.end()) {
					if (is_bound_buffer(variable_map[ptr_var_id].storage_class)) {
						auto binding = glsl.get_decoration(ptr_var_id, spv::DecorationBinding);
						shader.resource_binding_map[binding].read = true;
					}
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
			case SpvOpAtomicIAdd: {
				uint32_t store_id = insn[3];
				store_helper(store_id);
			} break;

			case SpvOpStore: {
				assert(word_count >= 3);
				uint32_t store_id = insn[1];

				store_helper(store_id);

				// Store pointers for the first time, create the hash map
				if (access_chain_map.find(insn[2]) != access_chain_map.end()) {
					const auto& access_chain = access_chain_map[insn[2]];
					std::string container_name;
					std::string ptr_name;
					std::string pointee_type_name;
					const uint32_t ptr_id = insn[1];
					auto var_name = glsl.get_name(ptr_id);
					auto ptr_type = glsl.get_type_from_variable(ptr_id);
					if (constant_map.find(access_chain.offset_idx) != constant_map.end()) {
						auto parent_type_id = glsl.get_type_from_variable(access_chain.base_ptr_id).parent_type;
						auto ptr_struct_type = glsl.get_type(parent_type_id);
						auto ptr_struct_name = glsl.get_name(parent_type_id);
						assert(ptr_struct_type.member_types.size());
						for (auto mem_type_id : ptr_struct_type.member_types) {
							container_name = glsl.get_name(mem_type_id);
							ptr_name =
								glsl.get_member_name(mem_type_id, glsl.get_constant(access_chain.offset_idx).scalar());
						}
						buffer_ptr_hash_map[ptr_id] = container_name + '_' + ptr_name;	//+ '_' + pointee_type_name;
					}
				}

			} break;
		}
		assert(insn + word_count <= code + code_size);
		insn += word_count;
	}
}

static void parse_shader(Shader& shader, const uint32_t* code, size_t code_size, lumen::RenderPass* pass) {
	spirv_cross::CompilerGLSL glsl(code, code_size);
	spirv_cross::ShaderResources resources = glsl.get_shader_resources();

	auto reflect = [&shader, &glsl](const spirv_cross::Resource& resource, VkDescriptorType type) {
		// unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
		unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		shader.binding_mask |= 1 << binding;
		shader.descriptor_types[binding] = type;
	};

	auto max = [](unsigned a, unsigned b) { return a > b ? a : b; };

	// Get entry point
	shader.stage = get_shader_stage(glsl.get_execution_model());
	// Record execution sizes
	shader.local_size_x = max(1u, glsl.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0));
	shader.local_size_y = max(1u, glsl.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1));
	shader.local_size_z = max(1u, glsl.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2));

	// Do reflection
	if (resources.push_constant_buffers.size() >= 1) {
		shader.uses_push_constants = true;
	}
	// Uniform buffer
	for (const auto& resource : resources.uniform_buffers) {
		reflect(resource, descriptor_Type_map[ResourceType::UniformBuffer]);
	}

	// Storage buffer
	for (const auto& resource : resources.storage_buffers) {
		reflect(resource, descriptor_Type_map[ResourceType::StorageBuffer]);
	}
	// Storage image
	for (const auto& resource : resources.storage_images) {
		reflect(resource, descriptor_Type_map[ResourceType::StorageImage]);
	}
	// Combined image sampler
	for (const auto& resource : resources.sampled_images) {
		reflect(resource, descriptor_Type_map[ResourceType::SampledImage]);
	}
	// Acceleration structure
	if (resources.acceleration_structures.size()) {
		resources.acceleration_structures[0];
		auto set = glsl.get_decoration(resources.acceleration_structures[0].id, spv::DecorationDescriptorSet);
		auto binding = glsl.get_decoration(resources.acceleration_structures[0].id, spv::DecorationBinding);
		LUMEN_ASSERT(set == 1 && binding == 0, "Make sure the TLAS is bound to set 1, binding 0");
	}

	// Input attachments for vertex shader
	if (shader.stage == VK_SHADER_STAGE_VERTEX_BIT) {
		for (const auto& resource : resources.stage_inputs) {
			// auto attachment_idx = glsl.get_decoration(resource.id, spv::DecorationLocation);
			auto type = glsl.get_type(resource.type_id);
			auto base_type = glsl.get_type(resource.base_type_id);
			auto vec_size = type.vecsize;
			if (vertex_input_map.find({base_type.basetype, vec_size}) != vertex_input_map.end()) {
				shader.vertex_inputs.push_back(vertex_input_map[{base_type.basetype, vec_size}]);
			}
		}
	}
	LUMEN_ASSERT(resources.push_constant_buffers.size() <= 1,
				 "Only 1 push constant is supported per shader at the moment!");
	if (resources.push_constant_buffers.size()) {
		auto type = glsl.get_type(resources.push_constant_buffers[0].type_id);
		uint32_t pc_size = get_pc_size(glsl, type);
		shader.push_constant_size = pc_size;
	}
	if (pass->rg->settings.shader_inference) {
		parse_spirv(glsl, resources, shader, code, code_size, pass);
	}
}

#if USE_SHADERC

static std::unordered_map<std::string, shaderc_shader_kind> mstages = {
	{"vert", shaderc_vertex_shader}, {"frag", shaderc_fragment_shader}, {"comp", shaderc_compute_shader},
	{"rgen", shaderc_raygen_shader}, {"rahit", shaderc_anyhit_shader},	{"rchit", shaderc_closesthit_shader},
	{"rmiss", shaderc_miss_shader},
};

static std::vector<uint32_t> compile_file(const std::string& source_name, shaderc_shader_kind kind,
										  const std::string& source, lumen::RenderPass* pass, bool optimize = false) {
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	auto add_macros = [&options](const std::vector<ShaderMacro>& macros) {
		for (const auto& macro : macros) {
			if (macro.has_val) {
				options.AddMacroDefinition(macro.name, std::to_string(macro.val));

			} else if (!macro.name.empty()) {
				options.AddMacroDefinition(macro.name);
			}
		}
	};
	add_macros(pass->macro_defines);
	add_macros(pass->rg->global_macro_defines);
	if (optimize) {
		options.SetOptimizationLevel(shaderc_optimization_level_size);
	}

	shaderc_util::FileFinder fileFinder;
	options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));
	options.SetTargetSpirv(shaderc_spirv_version_1_6);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, 2);
#if 1
	options.SetGenerateDebugInfo();
#endif

	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
		std::cerr << module.GetErrorMessage();
		return std::vector<uint32_t>();
	}

	return {module.cbegin(), module.cend()};
}
#endif

Shader::Shader(const std::string& filename) : filename(filename) {}
int Shader::compile(lumen::RenderPass* pass) {
	LUMEN_TRACE("Compiling shader: {0}", name_with_macros);
#if USE_SHADERC
	std::ifstream fin(filename);
	std::stringstream buffer;
	buffer << fin.rdbuf();
	auto get_ext = [](const std::string& str) -> std::string {
		auto fnd = str.rfind('.');
		assert(fnd != std::string::npos);
		return str.substr(fnd + 1);
	};
	buffer << "\n";
	const auto& str = buffer.str();
	// Compiling
	binary = compile_file(filename, mstages[get_ext(filename)], str, pass);
	parse_shader(*this, binary.data(), binary.size(), pass);
	return 0;
#else
	std::string file_path = filename + ".spv";
#ifdef _DEBUG
	auto str = std::string("glslangValidator.exe --target-env vulkan1.3 " + filename + " -V " + " -g " + " -o " +
						   filename + ".spv");

#else
	auto str =
		std::string("glslangValidator.exe --target-env vulkan1.3 " + filename + " -V " + " -o " + filename + ".spv");
#endif	//  NDEBUG

	binary.clear();
	int ret_val = std::system(str.data());
	std::ifstream bin(file_path, std::ios::ate | std::ios::binary);
	if (!bin.good() && ret_val) {
		LUMEN_ERROR(std::string("Shader compilation failed: " + filename).data());
		bin.close();
		return ret_val;
	} else if (ret_val) {
		LUMEN_WARN(std::string("Shader compilation failed, resuming from old shader: " + filename).data());
	}
	size_t file_size = (size_t)bin.tellg();
	bin.seekg(0);
	binary.resize(file_size / 4);
	bin.read((char*)binary.data(), file_size);
	bin.close();
	parse_shader(*this, binary.data(), file_size / 4, pass);
	return ret_val;
#endif
}

VkShaderModule Shader::create_vk_shader_module(const VkDevice& device) const {
	VkShaderModuleCreateInfo shader_module_CI{};
	shader_module_CI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_CI.codeSize = binary.size() * 4;
	shader_module_CI.pCode = (uint32_t*)binary.data();
	shader_module_CI.pNext = nullptr;

	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &shader_module_CI, nullptr, &shader_module) != VK_SUCCESS) {
		LUMEN_ERROR("Failed to create shader module!");
	}
	return shader_module;
}
}  // namespace lumen
