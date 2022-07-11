#include "LumenPCH.h"
#include "Shader.h"


#define USE_SHADERC 0


struct Id {
    uint32_t opcode;
    uint32_t type_id;
    uint32_t storage_class;
    uint32_t binding;
    uint32_t set;
};

static VkShaderStageFlagBits get_shader_stage(SpvExecutionModel executionModel) {
    switch (executionModel) {
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
        default:
            assert(!"Unsupported execution model");
            return VkShaderStageFlagBits(0);
    }
}

// https://github.com/zeux/niagara/blob/master/src/shaders.cpp
static void parse_shader(Shader& shader, const uint32_t* code, uint32_t code_size) {
    assert(code[0] == SpvMagicNumber);

    uint32_t id_bound = code[3];

    std::vector<Id> ids(id_bound);

    const uint32_t* insn = code + 5;

    while (insn != code + code_size) {
        uint16_t opcode = uint16_t(insn[0]);
        uint16_t word_count = uint16_t(insn[0] >> 16);

        switch (opcode) {
            case SpvOpEntryPoint:
            {
                assert(word_count >= 2);
                shader.stage = get_shader_stage(SpvExecutionModel(insn[1]));
            } break;
            case SpvOpExecutionMode:
            {
                assert(word_count >= 3);
                uint32_t mode = insn[2];

                switch (mode) {
                    case SpvExecutionModeLocalSize:
                        assert(word_count == 6);
                        shader.local_size_x = insn[3];
                        shader.local_size_y = insn[4];
                        shader.local_size_z = insn[5];
                        break;
                }
            } break;
            case SpvOpDecorate:
            {
                assert(word_count >= 3);

                uint32_t id = insn[1];
                assert(id < id_bound);

                switch (insn[2]) {
                    case SpvDecorationDescriptorSet:
                        assert(word_count == 4);
                        ids[id].set = insn[3];
                        break;
                    case SpvDecorationBinding:
                        assert(word_count == 4);
                        ids[id].binding = insn[3];
                        break;
                }
            } break;
            case SpvOpTypeStruct:
            case SpvOpTypeImage:
            case SpvOpTypeSampler:
            case SpvOpTypeSampledImage:
            {
                assert(word_count >= 2);

                uint32_t id = insn[1];
                assert(id < id_bound);

                assert(ids[id].opcode == 0);
                ids[id].opcode = opcode;
            } break;
            case SpvOpTypePointer:
            {
                assert(word_count == 4);

                uint32_t id = insn[1];
                assert(id < id_bound);

                assert(ids[id].opcode == 0);
                ids[id].opcode = opcode;
                ids[id].type_id = insn[3];
                ids[id].storage_class = insn[2];
            } break;
            case SpvOpVariable:
            {
                assert(word_count >= 4);

                uint32_t id = insn[2];
                assert(id < id_bound);

                assert(ids[id].opcode == 0);
                ids[id].opcode = opcode;
                ids[id].type_id = insn[1];
                ids[id].storage_class = insn[3];
    } break;
}

        assert(insn + word_count <= code + code_size);
        insn += word_count;
    }

    for (auto& id : ids) {
        if (id.opcode == SpvOpVariable && (id.storage_class == SpvStorageClassUniform || id.storage_class == SpvStorageClassUniformConstant || id.storage_class == SpvStorageClassStorageBuffer)) {
            // TODO: Different set support?
            assert(id.set == 0);
            assert(id.binding < 32);
            assert(ids[id.type_id].opcode == SpvOpTypePointer);
            assert((shader.binding_mask & (1 << id.binding)) == 0);

            uint32_t type_kind = ids[ids[id.type_id].type_id].opcode;
            switch (type_kind) {
                case SpvOpTypeStruct:
                    shader.descriptor_types[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    shader.binding_mask |= 1 << id.binding;
                    break;
                case SpvOpTypeImage:
                    shader.descriptor_types[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    shader.binding_mask |= 1 << id.binding;
                    break;
                case SpvOpTypeSampler:
                    shader.descriptor_types[id.binding] = VK_DESCRIPTOR_TYPE_SAMPLER;
                    shader.binding_mask |= 1 << id.binding;
                    break;
                case SpvOpTypeSampledImage:
                    shader.descriptor_types[id.binding] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    shader.binding_mask |= 1 << id.binding;
                    break;
                default:
                    assert(!"Unknown resource type");
            }
        }

        if (id.opcode == SpvOpVariable && id.storage_class == SpvStorageClassPushConstant) {
            shader.uses_push_constants = true;
        }
    }
}


#if USE_SHADERC
#include <shaderc/shaderc.hpp>
#include <libshaderc_util/file_finder.h>
#include <glslc/file_includer.h>

static std::unordered_map<std::string, shaderc_shader_kind> mstages = {
    {"vert", shaderc_vertex_shader},
    {"frag", shaderc_fragment_shader},
    {"comp", shaderc_compute_shader},
    {"rgen", shaderc_raygen_shader},
    {"rahit", shaderc_anyhit_shader},
    {"rchit", shaderc_closesthit_shader},
    {"rmiss", shaderc_miss_shader},
};


std::string preprocess_shader(const std::string& source_name,
                              shaderc_shader_kind kind,
                              const std::string& source) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    shaderc_util::FileFinder fileFinder;
    options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));
    options.SetTargetSpirv(shaderc_spirv_version_1_5);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, 2);

    // Like -DMY_DEFINE=1
    options.AddMacroDefinition("MY_DEFINE", "1");

    shaderc::PreprocessedSourceCompilationResult result =
        compiler.PreprocessGlsl(source, kind, source_name.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cerr << result.GetErrorMessage();
        return "";
    }

    return { result.cbegin(), result.cend() };
}

// Compiles a shader to SPIR-V assembly. Returns the assembly text
// as a string.
std::string compile_file_to_assembly(const std::string& source_name,
                                     shaderc_shader_kind kind,
                                     const std::string& source,
                                     bool optimize = false) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;



    // Like -DMY_DEFINE=1
    options.AddMacroDefinition("MY_DEFINE", "1");
    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    shaderc_util::FileFinder fileFinder;
    options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));
    options.SetTargetSpirv(shaderc_spirv_version_1_5);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, 2);

    shaderc::AssemblyCompilationResult result = compiler.CompileGlslToSpvAssembly(
        source, kind, source_name.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cerr << result.GetErrorMessage();
        return "";
    }

    return { result.cbegin(), result.cend() };
}

// Compiles a shader to a SPIR-V binary. Returns the binary as
// a vector of 32-bit words.
std::vector<uint32_t> compile_file(const std::string& source_name,
                                   shaderc_shader_kind kind,
                                   const std::string& source,
                                   bool optimize = false) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    // Like -DMY_DEFINE=1
    options.AddMacroDefinition("MY_DEFINE", "1");
    if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_size);

    shaderc_util::FileFinder fileFinder;
    options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));
    options.SetTargetSpirv(shaderc_spirv_version_1_5);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, 2);
   

    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cerr << module.GetErrorMessage();
        return std::vector<uint32_t>();
    }

    return { module.cbegin(), module.cend() };
}
#endif

Shader::Shader() {}
Shader::Shader(const std::string& filename) : filename(filename) {}
int Shader::compile() {
    LUMEN_TRACE("Compiling shader: {0}", filename);
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
    auto get_path = [](const std::string& str) -> std::string {
        auto fnd = str.rfind('/');
        assert(fnd != std::string::npos);
        return str.substr(0, fnd);
    };
    const auto& str = buffer.str();
    // Preprocessing
    {  
       /* auto preprocessed = preprocess_shader(
            filename,mstages[get_ext(filename)], str);
        std::cout << "Compiled a vertex shader resulting in preprocessed text:"
            << std::endl
            << preprocessed << std::endl;*/
    }
    // Compiling
    {  
      /*  auto assembly = compile_file_to_assembly(
            filename, mstages[get_ext(filename)], str);
        std::cout << "SPIR-V assembly:" << std::endl << assembly << std::endl;*/
       binary =
            compile_file(filename, mstages[get_ext(filename)], str);
     /*   std::cout << "Compiled to a binary module with " << binary.size()
            << " words." << std::endl;*/
    }
    return 0;
#else
    	std::string file_path = filename + ".spv";
    #ifdef NDEBUG
    	auto str = std::string("glslangValidator.exe --target-env vulkan1.2 " +
    						   filename + " -V " + " -o " + filename + ".spv");
    #else
    	auto str =
    		std::string("glslangValidator.exe --target-env vulkan1.2 " + filename +
    					" -V " + " -g " + " -o " + filename + ".spv");
    #endif //  NDEBUG
    
    	binary.clear();
    	int ret_val = std::system(str.data());
    	std::ifstream bin(file_path, std::ios::ate | std::ios::binary);
    	if (!bin.good() && ret_val) {
    		LUMEN_ERROR(
    			std::string("Shader compilation failed: " + filename).data());
    		bin.close();
    		return ret_val;
    	} else if (ret_val) {
    		LUMEN_WARN(std::string(
    			"Shader compilation failed, resuming from old shader: " +
    			filename)
    			.data());
    	}
    	size_t file_size = (size_t)bin.tellg();
    	bin.seekg(0);
    	binary.resize(file_size / 4);
    	bin.read((char*)binary.data(), file_size);
    	bin.close();
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
	if (vkCreateShaderModule(device, &shader_module_CI, nullptr,
		&shader_module) != VK_SUCCESS) {
		LUMEN_ERROR("Failed to create shader module!");
	}
	return shader_module;
}