#include "LumenPCH.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "RTScene.h"
#include <algorithm>
#include <chrono>
bool use_rtx = true;
bool use_vm = false;
bool use_vc = true;
bool delay_pt = false;
bool use_area_sampling = true;
float vcm_radius_factor = 0.1;
int integrator = 2;
float ppm_base_radius = 0.25;
const int max_depth = 5;
// TODO: Use instances in the rasterization pipeline
// TODO: Use a single scratch buffer
RTScene* RTScene::instance = nullptr;
static void fb_resize_callback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<RTScene*>(glfwGetWindowUserPointer(window));
	app->resized = true;
}

RTScene::RTScene(int width, int height, bool debug)
	: Scene(width, height, debug) {
	this->instance = this;
}

void RTScene::init(Window* window) {
	srand(time(NULL));
	this->window = window;
	vkb.ctx.window_ptr = window->get_window_ptr();
	glfwSetFramebufferSizeCallback(vkb.ctx.window_ptr, fb_resize_callback);
	// Init with ray tracing extensions
	vkb.add_device_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
	vkb.create_instance();
	if (vkb.enable_validation_layers) {
		vkb.setup_debug_messenger();
	}
	vkb.create_surface();
	vkb.pick_physical_device();
	vkb.create_logical_device();
	vkb.create_swapchain();
	create_default_render_pass(vkb.ctx);
	vkb.create_framebuffers(vkb.ctx.default_render_pass);
	vkb.create_command_pool();
	vkb.create_command_buffers();
	vkb.create_sync_primitives();
	initialized = true;

	// Requesting ray tracing properties
	VkPhysicalDeviceProperties2 prop2{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	prop2.pNext = &rt_props;
	vkGetPhysicalDeviceProperties2(vkb.ctx.physical_device, &prop2);

	init_scene();
	auto cam_ptr = camera.get();
	window->add_mouse_click_callback(
		[window, cam_ptr, this](MouseAction button, KeyAction action) {
		if (updated && window->is_mouse_up(MouseAction::LEFT)) {
			updated = true;
		}
		if (updated && window->is_mouse_down(MouseAction::LEFT)) {
			updated = true;
		}
	});
	window->add_mouse_move_callback(
		[window, cam_ptr, this](double delta_x, double delta_y) {
		if (window->is_mouse_held(MouseAction::LEFT)) {
			cam_ptr->rotate(0.05f * (float)delta_y, -0.05f * (float)delta_x,
							0.0f);
			//pc_ray.frame_num = -1;
			updated = true;
		}
	});
	create_offscreen_resources();
	create_descriptors();
	create_graphics_pipeline();
	create_uniform_buffers();
	update_descriptors();
	rt_initialized = use_rtx;
	if (use_rtx) {
		create_blas();
		create_tlas();
		create_rt_descriptors();
		create_rt_pipelines();
	}
	create_post_descriptor();
	update_post_desc_set();
	create_post_pipeline();
	create_compute_pipelines();

	init_imgui();
}

void RTScene::init_scene() {
	constexpr int VERTEX_BINDING_ID = 0;
	camera = std::unique_ptr<PerspectiveCamera>(
		new PerspectiveCamera(45.0f, 0.01f, 1000.0f, (float)width / height,
		glm::vec3(0.7, 0.5, 15.5)));
	pc_ray.total_light_area = 0;
	//std::string filename = "scenes/Sponza/glTF/Sponza.gltf";
	std::string filename = "scenes/cornellBox.gltf";
	//std::string filename = "scenes/scene3.gltf";
	using vkBU = VkBufferUsageFlagBits;
	tinygltf::Model tmodel;
	tinygltf::TinyGLTF tcontext;
	std::string warn, error;
	LUMEN_TRACE("Loading file: {}", filename.c_str());
	if (!tcontext.LoadASCIIFromFile(&tmodel, &error, &warn, filename)) {
		assert(!"Error while loading scene");
	}
	if (!warn.empty()) {
		LUMEN_WARN(warn.c_str());
	}
	if (!error.empty()) {
		LUMEN_ERROR(error.c_str());
	}
	gltf_scene.import_materials(tmodel);
	gltf_scene.import_drawable_nodes(tmodel, GltfAttributes::Normal |
									 GltfAttributes::Texcoord_0);

	auto vertex_buf_size = gltf_scene.positions.size() * sizeof(glm::vec3);
	auto idx_buf_size = gltf_scene.indices.size() * sizeof(uint32_t);
	std::vector<MaterialPushConst> materials;
	std::vector<PrimMeshInfo> prim_lookup;
	for (const auto& m : gltf_scene.materials) {
		materials.push_back(
			{ m.base_color_factor, m.emissive_factor, m.base_color_texture });
	}
	uint32_t idx = 0;
	uint32_t total_light_triangle_cnt = 0;
	for (auto& pm : gltf_scene.prim_meshes) {
		prim_lookup.push_back({ pm.first_idx, pm.vtx_offset, pm.material_idx });
		auto& mef = materials[pm.material_idx].emissive_factor;
		if (mef.x > 0 || mef.y > 0 || mef.z > 0) {
			MeshLight light;
			light.num_triangles = pm.idx_count / 3;
			light.prim_mesh_idx = idx;
			lights.emplace_back(light);
			total_light_triangle_cnt += light.num_triangles;
		}
		idx++;
	}
	vertex_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		vertex_buf_size, gltf_scene.positions.data(), true);
	index_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		idx_buf_size, gltf_scene.indices.data(), true);

	normal_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		gltf_scene.normals.size() * sizeof(gltf_scene.normals[0]),
		gltf_scene.normals.data(), true);
	uv_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		gltf_scene.texcoords0.size() * sizeof(glm::vec2),
		gltf_scene.texcoords0.data(), true);
	materials_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		materials.size() * sizeof(GLTFMaterial), materials.data(), true);
	prim_lookup_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		prim_lookup.size() * sizeof(PrimMeshInfo), prim_lookup.data(), true);


	// BDPT related buffers, TODO: Add a check
	light_path_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * (max_depth + 1) * sizeof(PathVertex));

	camera_path_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * (max_depth + 2) * sizeof(PathVertex));

	path_backup_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * 7 * sizeof(VertexBackup));

	color_storage_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * 3 * sizeof(float)
	);
	sppm_data_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * sizeof(SPPMData) * sizeof(float)
	);

	atomic_data_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		sizeof(AtomicData) * sizeof(float)
	);

	atomic_data_cpu.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE,
		sizeof(AtomicData) * sizeof(float)
	);

	residual_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * 4 * sizeof(float)
	);

	counter_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		sizeof(int)
	);
	hash_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * sizeof(HashData)
	);

	photon_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		20 * width * height * sizeof(PhotonHash)
	);
	vcm_light_vertices_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width * height * max_depth * sizeof(VCMVertex)
	);

	light_path_cnt_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		width* height * 4
	);

	if (total_light_triangle_cnt) {
		light_vis_buffer.create(
			&vkb.ctx,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
			(VkDeviceSize)width * height * 2 *
			total_light_triangle_cnt * sizeof(float));
	}

	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	desc.camera_path_addr = camera_path_buffer.get_device_address();
	desc.light_path_addr = light_path_buffer.get_device_address();
	desc.path_backup_addr = path_backup_buffer.get_device_address();
	desc.color_storage_addr = color_storage_buffer.get_device_address();
	desc.sppm_data_addr = sppm_data_buffer.get_device_address();
	desc.atomic_data_addr = atomic_data_buffer.get_device_address();
	desc.residual_addr = residual_buffer.get_device_address();
	desc.counter_addr = counter_buffer.get_device_address();
	desc.hash_addr = hash_buffer.get_device_address();
	desc.photon_addr = photon_buffer.get_device_address();
	desc.vcm_light_vertices_addr = vcm_light_vertices_buffer.get_device_address();
	desc.light_path_cnt_addr = light_path_cnt_buffer.get_device_address();
	if (total_light_triangle_cnt) {
		desc.light_vis_addr = light_vis_buffer.get_device_address();
	}

	scene_desc_buffer.create(&vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
							 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc),
							 &desc, true);

	// Create a sampler for textures
	VkSamplerCreateInfo sampler_ci = vk::sampler_create_info();
	sampler_ci.minFilter = VK_FILTER_LINEAR;
	sampler_ci.magFilter = VK_FILTER_LINEAR;
	sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_ci.maxLod = FLT_MAX;
	vk::check(
		vkCreateSampler(vkb.ctx.device, &sampler_ci, nullptr, &texture_sampler),
		"Could not create image sampler");

	auto add_default_texture = [this]() {
		std::array<uint8_t, 4> nil = { 0, 0, 0, 0 };
		textures.resize(1);
		auto ci = make_img2d_ci(VkExtent2D{ 1, 1 });
		textures[0].load_from_data(&vkb.ctx, nil.data(), 4, ci,
								   texture_sampler);
	};

	if (tmodel.images.empty()) {
		add_default_texture();
	}
	else {
		textures.resize(tmodel.images.size());
		for (int i = 0; i < tmodel.images.size(); i++) {
			auto& gltf_img = tmodel.images[i];
			void* data = &gltf_img.image[0];
			VkDeviceSize size = gltf_img.image.size();
			auto img_dims =
				VkExtent2D{ (uint32_t)gltf_img.width, (uint32_t)gltf_img.height };

			if (size == 0 || gltf_img.width == -1 || gltf_img.height == -1) {
				add_default_texture();
				continue;
			}

			auto create_info = make_img2d_ci(img_dims, VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_USAGE_SAMPLED_BIT, true);
			textures[i].load_from_data(&vkb.ctx, data, size, create_info,
				texture_sampler);
		}
	}

	pc_ray.frame_num = -1;
	pc_ray.size_x = width;
	pc_ray.size_y = height;
}

void RTScene::create_blas() {
	std::vector<BlasInput> blas_inputs;
	auto vertex_address =
		get_device_address(vkb.ctx.device, vertex_buffer.handle);
	auto idx_address = get_device_address(vkb.ctx.device, index_buffer.handle);
	for (auto& primMesh : gltf_scene.prim_meshes) {
		BlasInput geo = to_vk_geometry(primMesh, vertex_address, idx_address);
		blas_inputs.push_back({ geo });
	}
	vkb.build_blas(blas_inputs,
				   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void RTScene::create_tlas() {
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	float total_light_triangle_area = 0.0f;
	int light_triangle_cnt = 0;
	for (auto& node : gltf_scene.nodes) {
		auto it = std::find_if(lights.begin(), lights.end(),
							   [&](const MeshLight& light) {
			return light.prim_mesh_idx == node.prim_mesh;
		});
		if (it != lights.end()) {
			auto idx = it - lights.begin();
			lights[idx].world_matrix = node.world_matrix;
			auto& prim_mesh = gltf_scene.prim_meshes[node.prim_mesh];
			auto& vtx_offset = prim_mesh.vtx_offset;
			auto& idx_base_offset = prim_mesh.first_idx;
			auto& vertices = gltf_scene.positions;
			auto& indices = gltf_scene.indices;
			for (int i = 0; i < prim_mesh.idx_count / 3; i++) {
				auto idx_offset = idx_base_offset + 3 * i;
				glm::ivec3 ind = { indices[idx_offset], indices[idx_offset + 1],
								  indices[idx_offset + 2] };
				ind += glm::vec3{ vtx_offset, vtx_offset, vtx_offset };
				const vec3 v0 =
					node.world_matrix * glm::vec4(vertices[ind.x], 1.0);
				const vec3 v1 =
					node.world_matrix * glm::vec4(vertices[ind.y], 1.0);
				const vec3 v2 =
					node.world_matrix * glm::vec4(vertices[ind.z], 1.0);
				auto area = 0.5 * glm::length(glm::cross(v1 - v0, v2 - v0));
				total_light_triangle_area += area;
				light_triangle_cnt++;
			}
		}

		VkAccelerationStructureInstanceKHR ray_inst{};
		ray_inst.transform = to_vk_matrix(node.world_matrix);
		ray_inst.instanceCustomIndex =
			node.prim_mesh; // gl_InstanceCustomIndexEXT: to find which
							// primitive
		ray_inst.accelerationStructureReference =
			vkb.get_blas_device_address(node.prim_mesh);
		ray_inst.flags =
			VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		ray_inst.mask = 0xFF;
		ray_inst.instanceShaderBindingTableRecordOffset =
			0; // We will use the same hit group for all objects
		tlas.emplace_back(ray_inst);
	}

	if (lights.size()) {
		mesh_lights_buffer.create(
			&vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
			lights.size() * sizeof(MeshLight), lights.data(), true);
	}

	pc_ray.total_light_area += total_light_triangle_area;
	if (light_triangle_cnt > 0) {
		pc_ray.light_triangle_count = light_triangle_cnt;
	}

	vkb.build_tlas(tlas,
				   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void RTScene::create_rt_descriptors() {
	constexpr int TLAS_BINDING = 0;
	constexpr int IMAGE_BINDING = 1;
	constexpr int INSTANCE_BINDING = 2;
	constexpr int LIGHTS_BINDING = 3;
	constexpr int TMP_IMG_BINDING = 4;

	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1) };

	auto descriptor_pool_ci =
		vk::descriptor_pool_CI(pool_sizes.size(), pool_sizes.data(), 2);
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci,
			  nullptr, &rt_desc_pool),
			  "Failed to create RT descriptor pool");
	// RT bindings
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			TLAS_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
			IMAGE_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			INSTANCE_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			LIGHTS_BINDING),
			vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			TMP_IMG_BINDING)
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(), set_layout_bindings.size());
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci,
			  nullptr, &rt_desc_layout),
			  "Failed to create RT descriptor set layout");

	VkDescriptorSetAllocateInfo set_allocate_info{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	set_allocate_info.descriptorPool = rt_desc_pool;
	set_allocate_info.descriptorSetCount = 1;
	set_allocate_info.pSetLayouts = &rt_desc_layout;
	vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info, &rt_desc_set);

	VkAccelerationStructureKHR tlas = vkb.tlas.accel;
	VkWriteDescriptorSetAccelerationStructureKHR desc_as_info{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	desc_as_info.accelerationStructureCount = 1;
	desc_as_info.pAccelerationStructures = &tlas;

	// TODO: Abstraction
	std::vector<VkWriteDescriptorSet> writes{
		vk::write_descriptor_set(rt_desc_set,
								 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 TLAS_BINDING, &desc_as_info),
		vk::write_descriptor_set(rt_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								 IMAGE_BINDING,
								 &offscreen_img.descriptor_image_info),
		vk::write_descriptor_set(rt_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								 INSTANCE_BINDING,
								 &prim_lookup_buffer.descriptor),
		vk::write_descriptor_set(rt_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								 TMP_IMG_BINDING,
								 &offscreen_tmp_img.descriptor_image_info)
	};
	if (lights.size()) {
		writes.push_back(vk::write_descriptor_set(
			rt_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, LIGHTS_BINDING,
			&mesh_lights_buffer.descriptor));
	}
	vkUpdateDescriptorSets(vkb.ctx.device, (uint32_t)writes.size(),
						   writes.data(), 0, nullptr);
}

void RTScene::create_rt_pipelines() {
	enum StageIndices {
		eRaygen,
		eMiss,
		eAMiss,
		eClosestHit,
		eAnyHit,
		eShaderGroupCount
	};

	RTPipelineSettings settings;

	std::vector<Shader> shaders{ {"src/shaders/pt_unidir.rgen"},
								{"src/shaders/pathtrace.rmiss"},
								{"src/shaders/pathtrace_shadow.rmiss"},
								{"src/shaders/pathtrace.rchit"},
								{"src/shaders/pathtrace.rahit"},
								{"src/shaders/integrators.rgen"} };
	for (auto& shader : shaders) {
		shader.compile();
	}
	settings.ctx = &vkb.ctx;
	settings.rt_props = rt_props;
	// All stages

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.resize(eShaderGroupCount);

	VkPipelineShaderStageCreateInfo stage{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage.pName = "main"; // All the same entry point
	// Raygen
	stage.module = shaders[5].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[eRaygen] = stage;
	// Miss
	stage.module = shaders[eMiss].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[eMiss] = stage;

	stage.module = shaders[eAMiss].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[eAMiss] = stage;
	// Hit Group - Closest Hit
	stage.module = shaders[eClosestHit].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[eClosestHit] = stage;
	// Hit Group - Any hit
	stage.module = shaders[eAnyHit].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	stages[eAnyHit] = stage;


	// Shader groups
	VkRayTracingShaderGroupCreateInfoKHR group{
		VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	group.anyHitShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = VK_SHADER_UNUSED_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.intersectionShader = VK_SHADER_UNUSED_KHR;

	// Raygen
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eRaygen;
	settings.groups.push_back(group);

	// Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eMiss;
	settings.groups.push_back(group);

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eAMiss;
	settings.groups.push_back(group);

	// closest hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = eClosestHit;
	settings.groups.push_back(group);

	// Any hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.anyHitShader = eAnyHit;
	settings.groups.push_back(group);

	settings.push_consts.push_back({ VK_SHADER_STAGE_RAYGEN_BIT_KHR |
										 VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
										 VK_SHADER_STAGE_MISS_BIT_KHR,
									 0, sizeof(PushConstantRay) });
	// Descriptor sets: one specific to ray tracing, and one shared with the
	// rasterization pipeline
	settings.desc_layouts = { rt_desc_layout,desc_set_layout };
	rt_pipelines.resize(Integrator::PIPELINE_COUNT);
	rt_pipelines[Integrator::PT] = std::make_unique<Pipeline>(vkb.ctx.device);
	rt_pipelines[Integrator::DELAYED_PT] = std::make_unique<Pipeline>(vkb.ctx.device);
	rt_pipelines[Integrator::BDPT] = std::make_unique<Pipeline>(vkb.ctx.device);
	rt_pipelines[Integrator::PPM] = std::make_unique<Pipeline>(vkb.ctx.device);
	rt_pipelines[Integrator::PPM+1] = std::make_unique<Pipeline>(vkb.ctx.device);
	rt_pipelines[Integrator::VCM] = std::make_unique<Pipeline>(vkb.ctx.device);
	rt_pipelines[Integrator::VCM + 1] = std::make_unique<Pipeline>(vkb.ctx.device);

	settings.stages = stages;
	rt_pipelines[Integrator::PT]->create_rt_pipeline(settings, { INTEGRATOR_PT });
	rt_pipelines[Integrator::BDPT]->create_rt_pipeline(settings, { INTEGRATOR_BDPT });
	rt_pipelines[Integrator::PPM]->create_rt_pipeline(settings, { INTEGRATOR_PPM_LIGHT });
	rt_pipelines[Integrator::PPM + 1]->create_rt_pipeline(settings, { INTEGRATOR_PPM_EYE });
	rt_pipelines[Integrator::VCM]->create_rt_pipeline(settings, { INTEGRATOR_VCM_LIGHT });
	rt_pipelines[Integrator::VCM + 1]->create_rt_pipeline(settings, { INTEGRATOR_VCM_EYE });
	stages[eRaygen].module = shaders[0].create_vk_shader_module(vkb.ctx.device);
	settings.stages = stages;
	rt_pipelines[Integrator::DELAYED_PT]->create_rt_pipeline(settings, { INTEGRATOR_PT });
	for (auto& s : settings.stages) {
		vkDestroyShaderModule(vkb.ctx.device, s.module, nullptr);
	}
}

void RTScene::create_compute_pipelines() {
	gather_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	min_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	min_reduce_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	max_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	max_reduce_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	calc_bounds_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	update_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	std::vector<Shader> shaders = {
		//{"src/shaders/gather.comp"},
		{"src/shaders/gpgpu/max.comp"},
		{"src/shaders/gpgpu/reduce_max.comp"},
		{"src/shaders/gpgpu/min.comp"},
		{"src/shaders/gpgpu/reduce_min.comp"},
		{"src/shaders/calc_bounds.comp"},
		//{"src/shaders/update.comp"},
	};
	for (auto& shader : shaders) {
		shader.compile();
	}
	std::vector<VkDescriptorSetLayout> desc_layouts = { rt_desc_layout,
														  desc_set_layout };
	//gather_pipeline->create_compute_pipeline(shaders[0], 2, desc_layouts.data(), {}, 4);
	min_pipeline->create_compute_pipeline(shaders[2], 2, desc_layouts.data(), {}, 4);
	min_reduce_pipeline->create_compute_pipeline(shaders[3], 2, desc_layouts.data(), {}, 4);
	max_pipeline->create_compute_pipeline(shaders[0], 2, desc_layouts.data(), {}, 4);
	max_reduce_pipeline->create_compute_pipeline(shaders[1], 2, desc_layouts.data(), {}, 4);
	calc_bounds_pipeline->create_compute_pipeline(shaders[4], 2, desc_layouts.data(), {}, 4);
	//update_pipeline->create_compute_pipeline(shaders[6], 2, desc_layouts.data(), {}, 4);
}

void RTScene::create_offscreen_resources() {
	// Create offscreen image for output
	TextureSettings settings;
	settings.usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	settings.base_extent = { (uint32_t)width, (uint32_t)height, 1 };
	settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	offscreen_img.create_empty_texture(&vkb.ctx, settings,
									   VK_IMAGE_LAYOUT_GENERAL);
	settings.usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	offscreen_tmp_img.create_empty_texture(&vkb.ctx, settings,
										   VK_IMAGE_LAYOUT_GENERAL);

	settings.usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	settings.format = VK_FORMAT_X8_D24_UNORM_PACK32;
	offscreen_depth.create_empty_texture(
		&vkb.ctx, settings, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	CommandBuffer cmd(&vkb.ctx, true);

	transition_image_layout(cmd.handle, offscreen_img.img,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	transition_image_layout(cmd.handle, offscreen_tmp_img.img,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	transition_image_layout(cmd.handle, offscreen_depth.img,
							VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							VK_IMAGE_ASPECT_DEPTH_BIT);
	cmd.submit();

	offscreen_renderpass =
		create_render_pass(vkb.ctx.device, { VK_FORMAT_R32G32B32A32_SFLOAT },
						   VK_FORMAT_X8_D24_UNORM_PACK32, 1, true, true,
						   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
	std::vector<VkImageView> attachments = { offscreen_img.img_view,
											offscreen_depth.img_view };

	vkDestroyFramebuffer(vkb.ctx.device, offscreen_framebuffer, nullptr);
	VkFramebufferCreateInfo info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	info.renderPass = offscreen_renderpass;
	info.attachmentCount = 2;
	info.pAttachments = attachments.data();
	info.width = width;
	info.height = height;
	info.layers = 1;
	vkCreateFramebuffer(vkb.ctx.device, &info, nullptr, &offscreen_framebuffer);
}

void RTScene::create_post_descriptor() {
	constexpr int SAMPLER_COLOR_BINDING = 0;
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
	auto descriptor_pool_ci =
		vk::descriptor_pool_CI(pool_sizes.size(), pool_sizes.data(), 1);
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci,
			  nullptr, &post_desc_pool),
			  "Failed to create descriptor pool");

	// Uniform buffer descriptors
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT, SAMPLER_COLOR_BINDING),
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(), set_layout_bindings.size());
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci,
			  nullptr, &post_desc_layout),
			  "Failed to create descriptor set layout");

	auto set_allocate_info =
		vk::descriptor_set_allocate_info(post_desc_pool, &post_desc_layout, 1);
	vk::check(vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info,
			  &post_desc_set),
			  "Failed to allocate descriptor sets");
}

void RTScene::create_post_pipeline() {
	GraphicsPipelineSettings post_settings;
	VkPipelineLayoutCreateInfo create_info{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	create_info.setLayoutCount = 1;
	create_info.pSetLayouts = &post_desc_layout;
	vkCreatePipelineLayout(vkb.ctx.device, &create_info, nullptr,
						   &post_pipeline_layout);

	post_settings.pipeline_layout = post_pipeline_layout;
	post_settings.render_pass = vkb.ctx.default_render_pass;
	post_settings.shaders = { {"src/shaders/post.vert"},
							 {"src/shaders/post.frag"} };
	for (auto& shader : post_settings.shaders) {
		if (shader.compile()) {
			LUMEN_ERROR("Shader compilation failed");
		}
	}
	post_settings.cull_mode = VK_CULL_MODE_NONE;
	post_settings.enable_tracking = false;
	post_settings.dynamic_state_enables = { VK_DYNAMIC_STATE_VIEWPORT,
										   VK_DYNAMIC_STATE_SCISSOR };
	post_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	post_pipeline->create_gfx_pipeline(post_settings);
}

void RTScene::update_post_desc_set() {
	auto write_desc_set = vk::write_descriptor_set(
		post_desc_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0,
		&offscreen_img.descriptor_image_info);
	vkUpdateDescriptorSets(vkb.ctx.device, 1, &write_desc_set, 0, nullptr);
}

double RTScene::draw_frame() {
	static const char* integrators[] = { "Unidirectional PT",
	"Bidirectional PT", "Progressive Photon Mapping" , "Vertex Connection And Merging"};
	auto t_begin = glfwGetTime() * 1000;
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Text("Frame time %f ms ( %f FPS )", cpu_avg_time,
		1000 / cpu_avg_time);
	updated ^= ImGui::Checkbox("Enable RTX", &use_rtx);
	updated ^= ImGui::Combo("Integrator", &integrator, integrators, IM_ARRAYSIZE(integrators));
	if (integrator == 0) {
		updated ^= ImGui::Checkbox("Use area sampling", &use_area_sampling);
		updated ^= ImGui::Checkbox("Delay NEE", &delay_pt);

	}

	if (integrator == 2) {
		updated ^= ImGui::SliderFloat("PPM Base Radius", &ppm_base_radius, 0.01, 1);
		AtomicData* data = (AtomicData*)atomic_data_cpu.data;
		if (data) {
			ImGui::Text("PPM Max Radius: %f", data->max_radius);
		}
	} else if(integrator == 3){
		if (use_vm) {
			ImGui::Text("VCM Photon Base Radius: %f",
				gltf_scene.m_dimensions.radius * vcm_radius_factor * 0.01);
			ImGui::Text("VCM Photon Current Radius: %f", pc_ray.radius);
			updated ^= ImGui::SliderFloat("VCM Radius %", &vcm_radius_factor, 0.01, 10);
		}
		updated ^= ImGui::Checkbox("Enable VM", &use_vm);
		updated ^= ImGui::Checkbox("Enable VC", &use_vc);
	}
	if (updated) {
		pc_ray.frame_num = -1;
		ImGui::Render();
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		return t_diff;
	}

	vk::check(vkWaitForFences(vkb.ctx.device, 1,
			  &vkb.in_flight_fences[vkb.current_frame], VK_TRUE,
			  1000000000),
			  "Timeout");
	uint32_t image_idx;
	VkResult result = vkAcquireNextImageKHR(
		vkb.ctx.device, vkb.ctx.swapchain, UINT64_MAX,
		vkb.image_available_sem[vkb.current_frame], VK_NULL_HANDLE, &image_idx);
	vk::check(vkResetCommandBuffer(vkb.ctx.command_buffers[image_idx], 0));
	if (result == VK_NOT_READY) {
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		return t_diff;
	}
	if (EventHandler::consume_event(LumenEvent::EVENT_SHADER_RELOAD)) {
		// We don't want any command buffers in flight, might change in the
		// future
		vkDeviceWaitIdle(vkb.ctx.device);
		for (auto& old_pipeline : EventHandler::obsolete_pipelines) {
			vkDestroyPipeline(vkb.ctx.device, old_pipeline, nullptr);
		}
		EventHandler::obsolete_pipelines.clear();
		vkb.create_command_buffers();
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		return t_diff;
	}

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		// Window resize
		vkb.recreate_swap_chain(create_default_render_pass, vkb.ctx);
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		return t_diff;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		LUMEN_ERROR("Failed to acquire new swap chain image");
	}

	if (vkb.images_in_flight[image_idx] != VK_NULL_HANDLE) {
		vkWaitForFences(vkb.ctx.device, 1, &vkb.images_in_flight[image_idx],
						VK_TRUE, UINT64_MAX);
	}
	vkb.images_in_flight[image_idx] = vkb.in_flight_fences[vkb.current_frame];

	render(image_idx);

	VkSubmitInfo submit_info = vk::submit_info();
	VkSemaphore wait_semaphores[] = {
		vkb.image_available_sem[vkb.current_frame] };
	VkPipelineStageFlags wait_stages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vkb.ctx.command_buffers[image_idx];

	VkSemaphore signal_semaphores[] = {
		vkb.render_finished_sem[vkb.current_frame] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	vkResetFences(vkb.ctx.device, 1, &vkb.in_flight_fences[vkb.current_frame]);

	vk::check(vkQueueSubmit(vkb.ctx.queues[(int)QueueType::GFX], 1,
			  &submit_info,
			  vkb.in_flight_fences[vkb.current_frame]),
			  "Failed to submit draw command buffer");
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swapchains[] = { vkb.ctx.swapchain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;

	present_info.pImageIndices = &image_idx;

	result = vkQueuePresentKHR(vkb.ctx.queues[(int)QueueType::PRESENT],
							   &present_info);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
		resized) {
		resized = false;
		vkb.recreate_swap_chain(create_default_render_pass, vkb.ctx);
	} else if (result != VK_SUCCESS) {
		LUMEN_ERROR("Failed to present swap chain image");
	}

	vkb.current_frame = (vkb.current_frame + 1) % vkb.MAX_FRAMES_IN_FLIGHT;
	auto t_end = glfwGetTime() * 1000;
	auto t_diff = t_end - t_begin;
	return t_diff;
}

void RTScene::create_descriptors() {
	constexpr int UNIFORM_BUFFER_BINDING = 0;
	constexpr int SCENE_DESC_BINDING = 1;
	constexpr int TEXTURES_BINDING = 2;

	auto num_textures = textures.size();
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								 num_textures),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
	};
	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(), pool_sizes.data(), vkb.ctx.swapchain_images.size());

	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci,
			  nullptr, &gfx_desc_pool),
			  "Failed to create descriptor pool");

	// Uniform buffer descriptors
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
										  VK_SHADER_STAGE_VERTEX_BIT |
											  VK_SHADER_STAGE_RAYGEN_BIT_KHR,
										  UNIFORM_BUFFER_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
				VK_SHADER_STAGE_COMPUTE_BIT,
			SCENE_DESC_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			TEXTURES_BINDING, num_textures) };
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(), set_layout_bindings.size());
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci,
			  nullptr, &desc_set_layout),
			  "Failed to create descriptor set layout");

	// Descriptor sets for the uniform matrices(For each swapchain image)
	auto set_layout_vec = std::vector<VkDescriptorSetLayout>(
		vkb.ctx.swapchain_images.size(), desc_set_layout);
	auto set_allocate_info = vk::descriptor_set_allocate_info(
		gfx_desc_pool, set_layout_vec.data(), vkb.ctx.swapchain_images.size());
	uniform_descriptor_sets.resize(vkb.ctx.swapchain_images.size());
	vk::check(vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info,
			  uniform_descriptor_sets.data()),
			  "Failed to allocate descriptor sets");
}

void RTScene::update_descriptors() {
	constexpr int UNIFORM_BUFFER_BINDING = 0;
	constexpr int SCENE_DESC_BINDING = 1;
	constexpr int TEXTURES_BINDING = 2;

	for (auto i = 0; i < uniform_descriptor_sets.size(); i++) {
		std::vector<VkWriteDescriptorSet> write_descriptor_sets{
			vk::write_descriptor_set(
				uniform_descriptor_sets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				UNIFORM_BUFFER_BINDING, &scene_ubo_buffer.descriptor),
			vk::write_descriptor_set(
				uniform_descriptor_sets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				SCENE_DESC_BINDING, &scene_desc_buffer.descriptor) };

		std::vector<VkDescriptorImageInfo> image_infos;
		for (auto& tex : textures) {
			image_infos.push_back(tex.descriptor_image_info);
		}
		write_descriptor_sets.push_back(vk::write_descriptor_set(
			uniform_descriptor_sets[i],
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TEXTURES_BINDING,
			image_infos.data(), (uint32_t)image_infos.size()));
		vkUpdateDescriptorSets(
			vkb.ctx.device, static_cast<uint32_t>(write_descriptor_sets.size()),
			write_descriptor_sets.data(), 0, nullptr);
	}
}

void RTScene::create_uniform_buffers() {
	scene_ubo_buffer.create(&vkb.ctx, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
							VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneUBO));
	update_uniform_buffers();
}

void RTScene::create_graphics_pipeline() {
	// Need to specify push constant range
	auto model_push_constant_range = vk::push_constant_range(
		VK_SHADER_STAGE_VERTEX_BIT, sizeof(ModelPushConst), 0);

	auto material_push_constant_range = vk::push_constant_range(
		VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(MaterialPushConst),
		sizeof(ModelPushConst));

	std::array<VkDescriptorSetLayout, 1> set_layouts = { desc_set_layout };
	auto pipeline_layout_ci = vk::pipeline_layout_CI(
		set_layouts.data(), static_cast<uint32_t>(set_layouts.size()));

	auto ranges = std::array<VkPushConstantRange, 2>{
		material_push_constant_range, model_push_constant_range};
	pipeline_layout_ci.pushConstantRangeCount = (uint32_t)ranges.size();
	pipeline_layout_ci.pPushConstantRanges = ranges.data();
	vk::check(vkCreatePipelineLayout(vkb.ctx.device, &pipeline_layout_ci,
			  nullptr, &gfx_pipeline_layout),
			  "Failed to create pipeline layout");
	gfx_pipeline_settings.binding_desc = {
		{0, sizeof(glm::vec3)}, {1, sizeof(glm::vec3)}, {2, sizeof(glm::vec2)} };
	gfx_pipeline_settings.attribute_desc = {
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // Position
		{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // Normal
		{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // UVO
	};
	gfx_pipeline_settings.dynamic_state_enables = { VK_DYNAMIC_STATE_VIEWPORT,
												   VK_DYNAMIC_STATE_SCISSOR };
	gfx_pipeline_settings.shaders = { {"src/shaders/gltf_simple.vert"},
									 {"src/shaders/gltf_simple.frag"} };
	for (auto& shader : gfx_pipeline_settings.shaders) {
		if (shader.compile()) {
			LUMEN_ERROR("Shader compilation failed");
		}
	}
	gfx_pipeline_settings.render_pass = offscreen_renderpass;
	gfx_pipeline_settings.front_face = VK_FRONT_FACE_CLOCKWISE;
	gfx_pipeline_settings.enable_tracking = false;

	gfx_pipeline_settings.pipeline_layout = gfx_pipeline_layout;
	gfx_pipeline_settings.pipeline = {};

	gfx_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	gfx_pipeline->create_gfx_pipeline(gfx_pipeline_settings);
}

void RTScene::update_uniform_buffers() {
	camera->update_view_matrix();
	scene_ubo.view = camera->view;
	scene_ubo.projection = camera->projection;
	scene_ubo.view_pos = glm::vec4(camera->position, 1);
	scene_ubo.inv_view = glm::inverse(camera->view);
	scene_ubo.inv_projection = glm::inverse(camera->projection);
	scene_ubo.model = glm::mat4(1.0);
	scene_ubo.light_pos = glm::vec4(3.0f, 2.5f, 1.0f, 1.0f);
	memcpy(scene_ubo_buffer.data, &scene_ubo, sizeof(scene_ubo));

	std::vector<VkWriteDescriptorSet> writes;
}

void RTScene::init_imgui() {
	VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000} };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &pool_info, nullptr,
			  &imgui_pool));
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Setup Platform/Renderer backends
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window->get_window_ptr(), true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = vkb.ctx.instance;
	init_info.PhysicalDevice = vkb.ctx.physical_device;
	init_info.Device = vkb.ctx.device;
	init_info.Queue = vkb.ctx.queues[(int)QueueType::GFX];
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, vkb.ctx.default_render_pass);

	CommandBuffer cmd(&vkb.ctx, true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(vkb.ctx.queues[(int)QueueType::GFX]);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void RTScene::render(uint32_t i) {
	auto sel_integrator = integrator_map[integrator];
	auto cmdbuf = vkb.ctx.command_buffers[i];
	auto draw_gltf = [this](VkCommandBuffer cmd, VkPipelineLayout layout,
							VkPipeline pipeline) {
		std::vector<VkDeviceSize> offsets = { 0, 0, 0 };
		std::vector<VkBuffer> buffers = {
			vertex_buffer.handle, normal_buffer.handle, uv_buffer.handle };

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
						  gfx_pipeline->handle);
		vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(buffers.size()),
							   buffers.data(), offsets.data());
		vkCmdBindIndexBuffer(cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
		uint32_t idx_node = 0;
		for (auto& node : gltf_scene.nodes) {
			auto& primitive = gltf_scene.prim_meshes[node.prim_mesh];
			auto& material = gltf_scene.materials[primitive.material_idx];
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
							   sizeof(glm::mat4), &node.world_matrix);
			material_push_const.base_color_factor = material.base_color_factor;
			material_push_const.texture_id = material.base_color_texture;
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT,
							   sizeof(ModelPushConst),
							   sizeof(MaterialPushConst), &material_push_const);
			vkCmdDrawIndexed(cmd, primitive.idx_count, 1, primitive.first_idx,
							 primitive.vtx_offset, 0);
		}
	};

	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vk::check(vkBeginCommandBuffer(cmdbuf, &begin_info));
	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	VkClearValue clear_depth = { 1.0f, 0 };
	VkViewport viewport = vk::viewport((float)width, (float)height, 0.0f, 1.0f);
	VkClearValue clear_values[] = { clear_color, clear_depth };

	// 1st pass
	if (use_rtx) {
		// Initializing push constant values
		glm::vec4 clear_color{ 1, 1, 1, 1 };
		pc_ray.clear_color = clear_color;
		pc_ray.light_pos = scene_ubo.light_pos;
		pc_ray.light_type = 0;
		pc_ray.light_intensity = 10;
		pc_ray.num_mesh_lights = lights.size();
		pc_ray.time = rand() % UINT_MAX;
		pc_ray.max_depth = max_depth;
		if (sel_integrator == Integrator::PPM) {
			pc_ray.radius = ppm_base_radius;
		}
		else if (sel_integrator == Integrator::VCM) {
			pc_ray.radius =gltf_scene.m_dimensions.radius * vcm_radius_factor / 100.f;
			pc_ray.radius /= pow(pc_ray.frame_num + 1, 0.5 * (1 - 2.0 / 3));
		}
		pc_ray.min_bounds = gltf_scene.m_dimensions.min;
		pc_ray.max_bounds = gltf_scene.m_dimensions.max;
		pc_ray.ppm_base_radius = ppm_base_radius;
		pc_ray.use_area_sampling = use_area_sampling;
		pc_ray.sky_col = vec3(0, 0, 0);
		pc_ray.use_vm = use_vm;
		pc_ray.use_vc = use_vc;
		const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
		const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
		const int base_grid_res = int(max_comp / pc_ray.radius);
		pc_ray.grid_res = glm::max(ivec3(diam * float(base_grid_res)/ max_comp), ivec3(1));
		if (pc_ray.radius < 0.0000001) pc_ray.radius = 0.0000001;
		std::vector<VkDescriptorSet> desc_sets{ rt_desc_set,
											   uniform_descriptor_sets[0] };
		if (sel_integrator == Integrator::PT && delay_pt) {
			sel_integrator = Integrator::DELAYED_PT;
		}
		// Trace rays
		VkBufferMemoryBarrier barrier;
		if (sel_integrator == Integrator::PPM) {
			vkCmdFillBuffer(cmdbuf, photon_buffer.handle, 0, photon_buffer.size, 0);
			barrier = buffer_barrier(photon_buffer.handle,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
			if (pc_ray.frame_num == 0) {
				AtomicData* data = (AtomicData*)atomic_data_cpu.data;
				data->max_radius = ppm_base_radius;
				vkCmdFillBuffer(cmdbuf, sppm_data_buffer.handle, 0, sppm_data_buffer.size, 0);
				barrier = buffer_barrier(sppm_data_buffer.handle,
					VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
					VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
				vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
			}
		} else if (sel_integrator == Integrator::VCM && use_vm) {
			vkCmdFillBuffer(cmdbuf, photon_buffer.handle, 0, photon_buffer.size, 0);
			barrier = buffer_barrier(photon_buffer.handle,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
		}
		// Trace rays from eye
		vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  rt_pipelines[sel_integrator]->handle);
		vkCmdBindDescriptorSets(
			cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipelines[sel_integrator]->pipeline_layout,
			0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
		vkCmdPushConstants(cmdbuf, rt_pipelines[sel_integrator]->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& regions = rt_pipelines[sel_integrator]->get_rt_regions();
		vkCmdTraceRaysKHR(cmdbuf, &regions[0], &regions[1], &regions[2], &regions[3], width, height, 1);

		if (sel_integrator == Integrator::PPM) {
			//const auto& ppm_light_pipeline = rt_pipelines[Integrator::PPM];
			const auto& ppm_eye_pipeline = rt_pipelines[Integrator::PPM+1];
			// Calc bbox of visible rays
	
			barrier = buffer_barrier(photon_buffer.handle,
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
		
			// Trace from eye
			vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
				ppm_eye_pipeline->handle);
			vkCmdBindDescriptorSets(
				cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, ppm_eye_pipeline->pipeline_layout,
				0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
			vkCmdPushConstants(cmdbuf, ppm_eye_pipeline->pipeline_layout,
				VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_MISS_BIT_KHR,
				0, sizeof(PushConstantRay), &pc_ray);
			auto& regions = ppm_eye_pipeline->get_rt_regions();
			vkCmdTraceRaysKHR(cmdbuf, &regions[0], &regions[1], &regions[2], &regions[3], width, height, 1);
			barrier = buffer_barrier(sppm_data_buffer.handle,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);

			auto wg_x = 1024;
			auto wg_y = 1;
			const auto num_wg_x = (uint32_t)ceil(width * height / float(wg_x));
			const auto num_wg_y = 1;
			vkCmdBindDescriptorSets(
				cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, max_pipeline->pipeline_layout,
				0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
			vkCmdBindDescriptorSets(
				cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, min_pipeline->pipeline_layout,
				0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
			vkCmdBindDescriptorSets(
				cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, max_reduce_pipeline->pipeline_layout,
				0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
			vkCmdBindDescriptorSets(
				cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, min_reduce_pipeline->pipeline_layout,
				0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
			// @Performance: Maybe we can have multiple residual buffers?
			reduce(cmdbuf, residual_buffer, counter_buffer, *max_pipeline, *max_reduce_pipeline,
				width * height);
			barrier = buffer_barrier(atomic_data_buffer.handle,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 1,
				&barrier, 0, 0);
			reduce(cmdbuf, residual_buffer, counter_buffer, *min_pipeline, *min_reduce_pipeline,
				width * height);
			barrier = buffer_barrier(atomic_data_buffer.handle,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 1,
				&barrier, 0, 0);
			vkCmdBindDescriptorSets(
				cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, calc_bounds_pipeline->pipeline_layout,
				0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
			vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE,
				calc_bounds_pipeline->handle);
			vkCmdDispatch(cmdbuf, 1, 1, 1);
			barrier = buffer_barrier(atomic_data_buffer.handle,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
			vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
				VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
			atomic_data_buffer.copy(atomic_data_cpu, cmdbuf);
		}
		if (sel_integrator == Integrator::VCM) {
			// TODO: VCM stuff
			const auto& vcm_light_pipeline = rt_pipelines[Integrator::VCM];
			const auto& vcm_eye_pipeline = rt_pipelines[Integrator::VCM + 1];
			if (use_vm) {
				barrier = buffer_barrier(photon_buffer.handle,
					VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
					VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
				vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
					VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
			}
			// Trace from eye
			vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
				vcm_eye_pipeline->handle);
			vkCmdBindDescriptorSets(
				cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vcm_eye_pipeline->pipeline_layout,
				0, (uint32_t)desc_sets.size(), desc_sets.data(), 0, nullptr);
			vkCmdPushConstants(cmdbuf, vcm_eye_pipeline->pipeline_layout,
				VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_MISS_BIT_KHR,
				0, sizeof(PushConstantRay), &pc_ray);
			auto& regions = vcm_eye_pipeline->get_rt_regions();
			vkCmdTraceRaysKHR(cmdbuf, &regions[0], &regions[1], &regions[2], &regions[3], width, height, 1);
		}
	} else {
		VkRenderPassBeginInfo gfx_rpi = vk::render_pass_begin_info();
		gfx_rpi.renderPass = offscreen_renderpass;
		gfx_rpi.framebuffer = offscreen_framebuffer;
		gfx_rpi.renderArea = { {0, 0}, vkb.ctx.swapchain_extent };
		gfx_rpi.clearValueCount = 2;
		gfx_rpi.pClearValues = &clear_values[0];

		vkCmdBeginRenderPass(cmdbuf, &gfx_rpi, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

		VkRect2D scissor = vk::rect2D(width, height, 0, 0);
		vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
								gfx_pipeline_layout, 0, 1,
								&uniform_descriptor_sets[i], 0, nullptr);
		draw_gltf(cmdbuf, gfx_pipeline_layout, gfx_pipeline->handle);

		vkCmdEndRenderPass(cmdbuf);
	}
	// 2nd pass
	VkRenderPassBeginInfo post_rpi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	post_rpi.clearValueCount = 2;
	post_rpi.pClearValues = clear_values;
	post_rpi.renderPass = vkb.ctx.default_render_pass;
	post_rpi.framebuffer = vkb.ctx.swapchain_framebuffers[i];
	post_rpi.renderArea = { {0, 0}, vkb.ctx.swapchain_extent };

	vkCmdBeginRenderPass(cmdbuf, &post_rpi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
	VkRect2D scissor = vk::rect2D(width, height, 0, 0);
	vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
	vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
					  post_pipeline->handle);
	vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
							post_pipeline_layout, 0, 1, &post_desc_set, 0,
							nullptr);
	vkCmdDraw(cmdbuf, 3, 1, 0, 0);
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
	vkCmdEndRenderPass(cmdbuf);
	VkClearColorValue val = { 0,0,0,1 };
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	auto clear_color_barrier = image_barrier(offscreen_tmp_img.img, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
											 offscreen_img.layout, offscreen_img.layout, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmdbuf, offscreen_tmp_img.img, offscreen_img.layout, &val, 1, &range);
	//vkCmdFillBuffer(cmdbuf, color_storage_buffer.handle, 0, color_storage_buffer.size, 0);
	auto fbb = buffer_barrier(color_storage_buffer.handle, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &clear_color_barrier);
	vk::check(vkEndCommandBuffer(cmdbuf), "Failed to record command buffer");
}

void RTScene::update() {

	pc_ray.frame_num++;
	double frame_time = draw_frame();

	cpu_avg_time = 0.95 * cpu_avg_time + 0.05 * frame_time;
	glm::vec3 translation{};
	float trans_speed = 0.01f;
	glm::vec3 front;
	if (window->is_key_held(KeyInput::KEY_LEFT_SHIFT)) {
		trans_speed *= 4;
	}

	front.x = cos(glm::radians(camera->rotation.x)) *
		sin(glm::radians(camera->rotation.y));
	front.y = sin(glm::radians(camera->rotation.x));
	front.z = cos(glm::radians(camera->rotation.x)) *
		cos(glm::radians(camera->rotation.y));
	front = glm::normalize(-front);
	if (window->is_key_held(KeyInput::KEY_W)) {
		camera->position += front * trans_speed;
		updated = true;
	}
	if (window->is_key_held(KeyInput::KEY_A)) {
		camera->position -=
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
			trans_speed;
		updated = true;
	}
	if (window->is_key_held(KeyInput::KEY_S)) {
		camera->position -= front * trans_speed;
		updated = true;
	}
	if (window->is_key_held(KeyInput::KEY_D)) {
		camera->position +=
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
			trans_speed;
		updated = true;
	}
	if (window->is_key_held(KeyInput::SPACE)) {
		// Right
		auto right =
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position += up * trans_speed;
		updated = true;
	}
	if (window->is_key_held(KeyInput::KEY_LEFT_CONTROL)) {
		auto right =
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position -= up * trans_speed;
		updated = true;
	}
	if (updated) {
		pc_ray.frame_num = -1;
		updated = false;
	}
	update_uniform_buffers();
}

void RTScene::cleanup() {
	auto device = vkb.ctx.device;
	bool rt = use_rtx || rt_initialized;
	vkDeviceWaitIdle(device);
	std::vector<Buffer> buffer_list = {
		vertex_buffer,     normal_buffer,    uv_buffer,
		index_buffer,      materials_buffer, prim_lookup_buffer,
		scene_desc_buffer, scene_ubo_buffer, light_vis_buffer,
		light_path_buffer, camera_path_buffer, path_backup_buffer,
		color_storage_buffer, sppm_data_buffer, atomic_data_buffer,
		residual_buffer, counter_buffer, hash_buffer, photon_buffer,
		vcm_light_vertices_buffer, light_path_cnt_buffer, atomic_data_cpu
	};

	std::vector<Pipeline*> pipeline_list = {
		gfx_pipeline.get(),
		post_pipeline.get(),
		gather_pipeline.get(),
		min_pipeline.get(),
		max_pipeline.get(),
		min_reduce_pipeline.get(),
		max_reduce_pipeline.get(),
		calc_bounds_pipeline.get(),
		update_pipeline.get()
	};
	if (lights.size()) {
		buffer_list.push_back(mesh_lights_buffer);
	}
	for (auto& b : buffer_list) {
		b.destroy();
	}
	offscreen_img.destroy();
	offscreen_tmp_img.destroy();
	offscreen_depth.destroy();

	for (auto& tex : textures) {
		tex.destroy();
	}
	vkDestroySampler(vkb.ctx.device, texture_sampler, nullptr);

	vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(device, post_desc_layout, nullptr);

	vkDestroyDescriptorPool(device, gfx_desc_pool, nullptr);
	vkDestroyDescriptorPool(device, post_desc_pool, nullptr);
	vkDestroyDescriptorPool(device, imgui_pool, nullptr);

	vkDestroyPipelineLayout(device, gfx_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, post_pipeline_layout, nullptr);

	vkDestroyRenderPass(device, offscreen_renderpass, nullptr);
	vkDestroyFramebuffer(device, offscreen_framebuffer, nullptr);
	for (auto p : pipeline_list) {
		if (p) {
			p->cleanup();
		}
	}
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	if (rt) {
		for (auto& p: rt_pipelines) {
			p->cleanup();

		}
		vkDestroyDescriptorSetLayout(device, rt_desc_layout, nullptr);
		vkDestroyDescriptorPool(device, rt_desc_pool, nullptr);
	}
	if (initialized) {
		vkb.cleanup();
	}
}
