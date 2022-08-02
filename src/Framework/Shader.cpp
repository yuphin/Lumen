#include "LumenPCH.h"
#include "Shader.h"

#include <spirv_cross/spirv_cross_c.h>
#define USE_SHADERC 1

static std::unordered_map< spvc_resource_type, VkDescriptorType> descriptor_Type_map = {
	{SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
	{SPVC_RESOURCE_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
	{SPVC_RESOURCE_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
	{SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
	{SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
};

struct input_map_hash {
	template <class T1, class T2>
	uint64_t operator () (const std::pair<T1, T2>& p) const {
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);
		return h1 << 16 | h2;
	}
};

static std::unordered_map <std::pair<spvc_basetype, uint32_t>, std::pair<VkFormat, size_t>, input_map_hash> vertex_input_map = {
	{{SPVC_BASETYPE_INT32, 1u}, {VK_FORMAT_R32_SINT, sizeof(int)}},
	{{SPVC_BASETYPE_INT32, 2u}, {VK_FORMAT_R32G32_SINT, 2 * sizeof(int)}},
	{{SPVC_BASETYPE_INT32, 3u}, {VK_FORMAT_R32G32B32_SINT, 3 * sizeof(int)}},
	{{SPVC_BASETYPE_INT32, 4u}, {VK_FORMAT_R32G32B32A32_SINT, 4 * sizeof(int)}},
	{{SPVC_BASETYPE_UINT32, 1u}, {VK_FORMAT_R32_UINT, sizeof(int)}},
	{{SPVC_BASETYPE_UINT32, 2u}, {VK_FORMAT_R32G32_UINT, 2 * sizeof(int)}},
	{{SPVC_BASETYPE_UINT32, 3u}, {VK_FORMAT_R32G32B32_UINT, 3 * sizeof(int)}},
	{{SPVC_BASETYPE_UINT32, 4u}, {VK_FORMAT_R32G32B32A32_UINT, 4 * sizeof(int)}},
	{{SPVC_BASETYPE_INT16, 1u}, {VK_FORMAT_R16_SINT, sizeof(int) / 2}},
	{{SPVC_BASETYPE_INT16, 2u}, {VK_FORMAT_R16G16_SINT, 2 * sizeof(int) / 2}},
	{{SPVC_BASETYPE_INT16, 3u}, {VK_FORMAT_R16G16B16_SINT, 3 * sizeof(int) / 2}},
	{{SPVC_BASETYPE_INT16, 4u}, {VK_FORMAT_R16G16B16A16_SINT, 4 * sizeof(int) / 2}},
	{{SPVC_BASETYPE_UINT16, 1u}, {VK_FORMAT_R16_UINT, sizeof(int) / 2}},
	{{SPVC_BASETYPE_UINT16, 2u}, {VK_FORMAT_R16G16_UINT, 2 * sizeof(int) / 2}},
	{{SPVC_BASETYPE_UINT16, 3u}, {VK_FORMAT_R16G16B16_UINT, 3 * sizeof(int) / 2}},
	{{SPVC_BASETYPE_UINT16, 4u}, {VK_FORMAT_R16G16B16A16_UINT, 4 * sizeof(int) / 2}},
	{{SPVC_BASETYPE_FP32, 1u},	{VK_FORMAT_R32_SFLOAT, sizeof(float)}},
	{{SPVC_BASETYPE_FP32, 2u}, {VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float)}},
	{{SPVC_BASETYPE_FP32, 3u}, {VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)}},
	{{SPVC_BASETYPE_FP32, 4u}, {VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeof(float)}},
	{{SPVC_BASETYPE_FP16, 1u},	{VK_FORMAT_R16_SFLOAT, sizeof(float) / 2}},
	{{SPVC_BASETYPE_FP16, 2u}, {VK_FORMAT_R16G16_SFLOAT, 2 * sizeof(float) / 2}},
	{{SPVC_BASETYPE_FP16, 3u}, {VK_FORMAT_R16G16B16_SFLOAT, 3 * sizeof(float) / 2}},
	{{SPVC_BASETYPE_FP16, 4u}, {VK_FORMAT_R16G16B16A16_SFLOAT, 4 * sizeof(float) / 2}},
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

static void spvc_err(void* data, const char* error) {
	LUMEN_ERROR("{}", error);
}

static void parse_shader(Shader& shader, const uint32_t* code, size_t code_size) {


	const SpvId* spirv = code;
	size_t word_count = code_size;

	spvc_context context = nullptr;
	spvc_parsed_ir ir = nullptr;
	spvc_compiler compiler_glsl = nullptr;
	spvc_resources resources = nullptr;
	const spvc_reflected_resource* list = nullptr;

	auto reflect = [&shader, &compiler_glsl](const spvc_reflected_resource* list, size_t count, VkDescriptorType type) {

		for (int i = 0; i < count; i++) {
			/*	printf("ID: %u, BaseTypeID: %u, TypeID: %u, Name: %s\n", list[i].id, list[i].base_type_id, list[i].type_id,
					   list[i].name);*/
			auto set = spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationDescriptorSet);
			auto binding = spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationBinding);
			//printf("  Set: %u, Binding: %u\n", set, binding);
			shader.binding_mask |= 1 << binding;
			shader.descriptor_types[binding] = type;
		}
	};

	auto max = [](unsigned a, unsigned b) {
		return a > b ? a : b;
	};

	// Create context.
	spvc_context_create(&context);
	// Set debug callback.
	spvc_context_set_error_callback(context, spvc_err, nullptr);
	// Parse the SPIR-V.
	spvc_context_parse_spirv(context, spirv, word_count, &ir);
	// Hand it off to a compiler instance and give it ownership of the IR.
	spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_glsl);

	// Get entry point
	const spvc_entry_point* entry_points;
	size_t num_entry_points;
	spvc_compiler_get_entry_points(compiler_glsl, &entry_points, &num_entry_points);
	LUMEN_ASSERT(num_entry_points == 1, "No entry point found in the shader");
	shader.stage = get_shader_stage(entry_points[0].execution_model);
	// Record execution sizes
	shader.local_size_x = max(1u, spvc_compiler_get_execution_mode_argument_by_index(compiler_glsl, SpvExecutionModeLocalSize, 0));
	shader.local_size_y = max(1u, spvc_compiler_get_execution_mode_argument_by_index(compiler_glsl, SpvExecutionModeLocalSize, 1));
	shader.local_size_z = max(1u, spvc_compiler_get_execution_mode_argument_by_index(compiler_glsl, SpvExecutionModeLocalSize, 2));
	// Check push constant
	auto resource_type = SPVC_RESOURCE_TYPE_PUSH_CONSTANT;
	size_t count;

	// Do reflection
	spvc_compiler_create_shader_resources(compiler_glsl, &resources);
	spvc_resources_get_resource_list_for_type(resources, resource_type, &list, &count);
	if (count >= 1) {
		shader.uses_push_constants = true;
	}

	// Uniform buffer
	resource_type = SPVC_RESOURCE_TYPE_UNIFORM_BUFFER;
	spvc_resources_get_resource_list_for_type(resources, resource_type, &list, &count);
	reflect(list, count, descriptor_Type_map[resource_type]);
	// Storage buffer
	resource_type = SPVC_RESOURCE_TYPE_STORAGE_BUFFER;
	spvc_resources_get_resource_list_for_type(resources, resource_type, &list, &count);
	reflect(list, count, descriptor_Type_map[resource_type]);
	// Storage image
	resource_type = SPVC_RESOURCE_TYPE_STORAGE_IMAGE;
	spvc_resources_get_resource_list_for_type(resources, resource_type, &list, &count);
	reflect(list, count, descriptor_Type_map[resource_type]);
	//// Combined image sampler
	//const spvc_combined_image_sampler* samplers;
	//size_t num_samplers;
	//spvc_compiler_get_combined_image_samplers(compiler_glsl, &samplers, &num_samplers);
	// Sampled image
	resource_type = SPVC_RESOURCE_TYPE_SAMPLED_IMAGE;
	spvc_resources_get_resource_list_for_type(resources, resource_type, &list, &count);
	reflect(list, count, descriptor_Type_map[resource_type]);
	// Acceleration structure
	resource_type = SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE;
	spvc_resources_get_resource_list_for_type(resources, resource_type, &list, &count);
	if (count > 0) {
		auto set = spvc_compiler_get_decoration(compiler_glsl, list[0].id, SpvDecorationDescriptorSet);
		auto binding = spvc_compiler_get_decoration(compiler_glsl, list[0].id, SpvDecorationBinding);
		LUMEN_ASSERT(set == 1 && binding == 0, "Make sure the TLAS is bound to set 1, binding 0");
	}

	//reflect(list, count, descriptor_Type_map[resource_type]);

	// Input attachments for vertex shader
	if (shader.stage == VK_SHADER_STAGE_VERTEX_BIT) {
		resource_type = SPVC_RESOURCE_TYPE_STAGE_INPUT;
		spvc_resources_get_resource_list_for_type(resources, resource_type, &list, &count);
		for (size_t i = 0; i < count; i++) {
			auto attachment_idx = spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationLocation);
			auto type = spvc_compiler_get_type_handle(compiler_glsl, list[i].type_id);
			auto base_type = spvc_type_get_basetype(type);
			auto vec_size = spvc_type_get_vector_size(type);
			if (vertex_input_map.find({ base_type, vec_size }) != vertex_input_map.end()) {
				shader.vertex_inputs.push_back(vertex_input_map[{base_type, vec_size}]);
			}
		}
	}

	//// Modify options.
	//const char* result = NULL;
	//spvc_compiler_options options = NULL;
	//spvc_compiler_create_compiler_options(compiler_glsl, &options);
	//spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 460);
	//spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_TRUE);
	//spvc_compiler_install_compiler_options(compiler_glsl, options);

	//spvc_compiler_compile(compiler_glsl, &result);
	//printf("Cross-compiled source: %s\n", result);

	// Frees all memory we allocated so far.
	spvc_context_destroy(context);
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
	options.SetTargetSpirv(shaderc_spirv_version_1_6);
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
	options.SetTargetSpirv(shaderc_spirv_version_1_6);
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
	options.SetTargetSpirv(shaderc_spirv_version_1_6);
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
	parse_shader(*this, binary.data(), binary.size());
	return 0;
#else
	std::string file_path = filename + ".spv";
#ifdef _DEBUG
	auto str =
		std::string("glslangValidator.exe --target-env vulkan1.3 " + filename +
					" -V " + " -g " + " -o " + filename + ".spv");

#else
	auto str = std::string("glslangValidator.exe --target-env vulkan1.3 " +
						   filename + " -V " + " -o " + filename + ".spv");
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
	parse_shader(*this, binary.data(), file_size / 4);
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