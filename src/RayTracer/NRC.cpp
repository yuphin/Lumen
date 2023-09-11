#include "LumenPCH.h"
#include "NRC.h"

#include <vulkan/vulkan_win32.h>
#include <VersionHelpers.h>

void *getMemHandle(VkDevice device, VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagBits handleType) {
#ifdef _WIN64
	HANDLE handle = 0;

	VkMemoryGetWin32HandleInfoKHR vkMemoryGetWin32HandleInfoKHR = {};
	vkMemoryGetWin32HandleInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
	vkMemoryGetWin32HandleInfoKHR.pNext = NULL;
	vkMemoryGetWin32HandleInfoKHR.memory = memory;
	vkMemoryGetWin32HandleInfoKHR.handleType = handleType;

	PFN_vkGetMemoryWin32HandleKHR fpGetMemoryWin32HandleKHR;
	//fpGetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR");
	//if (!fpGetMemoryWin32HandleKHR) {
	//	throw std::runtime_error("Failed to retrieve vkGetMemoryWin32HandleKHR!");
	//}

	//PFN_vkGetMemoryWin32HandleKHR()
	//vkGetMemoryWin32HandleKHR()
	if (vkGetMemoryWin32HandleKHR(device, &vkMemoryGetWin32HandleInfoKHR, &handle) != VK_SUCCESS) {
		throw std::runtime_error("Failed to retrieve handle for buffer!");
	}
	return (void *)handle;
#else
	int fd = -1;

	VkMemoryGetFdInfoKHR vkMemoryGetFdInfoKHR = {};
	vkMemoryGetFdInfoKHR.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
	vkMemoryGetFdInfoKHR.pNext = NULL;
	vkMemoryGetFdInfoKHR.memory = memory;
	vkMemoryGetFdInfoKHR.handleType = handleType;

	PFN_vkGetMemoryFdKHR fpGetMemoryFdKHR;
	fpGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(m_device, "vkGetMemoryFdKHR");
	if (!fpGetMemoryFdKHR) {
		throw std::runtime_error("Failed to retrieve vkGetMemoryWin32HandleKHR!");
	}
	if (fpGetMemoryFdKHR(m_device, &vkMemoryGetFdInfoKHR, &fd) != VK_SUCCESS) {
		throw std::runtime_error("Failed to retrieve handle for buffer!");
	}
	return (void *)(uintptr_t)fd;
#endif /* _WIN64 */
}

static void importCudaExternalMemory(void **cudaPtr, cudaExternalMemory_t &cudaMem, VkDeviceMemory &vkMem,
									 VkDeviceSize size, VkExternalMemoryHandleTypeFlagBits handleType,
									 VkDevice device) {
	cudaExternalMemoryHandleDesc externalMemoryHandleDesc = {};

	if (handleType & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT) {
		externalMemoryHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32;
	} else if (handleType & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT) {
		externalMemoryHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32Kmt;
	} else if (handleType & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) {
		externalMemoryHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueFd;
	} else {
		throw std::runtime_error("Unknown handle type requested!");
	}

	externalMemoryHandleDesc.size = size;

#ifdef _WIN64
	externalMemoryHandleDesc.handle.win32.handle = (HANDLE)getMemHandle(device, vkMem, handleType);
#else
	externalMemoryHandleDesc.handle.fd = (int)(uintptr_t)getMemHandle(vkMem, handleType);
#endif

	checkCudaErrors(cudaImportExternalMemory(&cudaMem, &externalMemoryHandleDesc));

	cudaExternalMemoryBufferDesc externalMemBufferDesc = {};
	externalMemBufferDesc.offset = 0;
	externalMemBufferDesc.size = size;
	externalMemBufferDesc.flags = 0;

	checkCudaErrors(cudaExternalMemoryGetMappedBuffer(cudaPtr, cudaMem, &externalMemBufferDesc));
}

static VkExternalMemoryHandleTypeFlagBits getDefaultMemHandleType() {
#ifdef _WIN64
	return IsWindows8Point1OrGreater() ? VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
									   : VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
#else
	return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif /* _WIN64 */
}

void NRC::init() {
	Integrator::init();
	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();

	test_buffer.create(&instance->vkb.ctx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
						   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
					   instance->width * instance->height * sizeof(float));

	scene_desc_buffer.create("Scene Desc", &instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc), &desc,
							 true);

	importCudaExternalMemory((void **)&cuda_mem_ptr, cuda_mem, test_buffer.buffer_memory,
							 instance->width * instance->height * sizeof(float), getDefaultMemHandleType(),
							 instance->vk_ctx.device);


	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	assert(instance->vkb.rg->settings.shader_inference == true);
	// For shader resource dependency inference, use this macro to register a buffer address to the rendergraph
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &prim_lookup_buffer, instance->vkb.rg);
}

void NRC::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pc_ray.num_lights = (int)lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;
	pc_ray.dir_light_idx = lumen_scene->dir_light_idx;
	instance->vkb.rg
		->add_rt("NRC", {.shaders = {{"src/shaders/integrators/nrc/nrc.rgen"},
									 {"src/shaders/ray.rmiss"},
									 {"src/shaders/ray_shadow.rmiss"},
									 {"src/shaders/ray.rchit"},
									 {"src/shaders/ray.rahit"}},
						 .dims = {instance->width, instance->height},
						 .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind({
			output_tex,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		//.write(output_tex) // Needed if the automatic shader inference is disabled
		.bind_tlas(instance->vkb.tlas);
	instance->vkb.rg->run_and_submit(cmd);
}

bool NRC::update() {
	pc_ray.frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}

void NRC::destroy() { Integrator::destroy(); }
