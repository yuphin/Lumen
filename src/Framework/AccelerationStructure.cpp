
#include "LumenPCH.h"
#include "AccelerationStructure.h"
#include "VkUtils.h"
#include "PersistentResourceManager.h"
#include "DynamicResourceManager.h"

namespace vk {
struct BuildAccelerationStructure {
	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	const VkAccelerationStructureBuildRangeInfoKHR* range_info;
	BVH as;	 // result acceleration structure
	BVH cleanup_as;
};

inline static bool has_flag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }

static BVH create_acceleration(VkAccelerationStructureCreateInfoKHR& accel) {
	BVH result_accel;
	// Allocating the buffer to hold the acceleration structure
	// Setting the buffer
	result_accel.buffer = prm::get_buffer(
		{.name = "Blas Buffer",
		 .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		 .memory_type = vk::BufferType::GPU,
		 .size = accel.size});
	accel.buffer = result_accel.buffer->handle;
	// Create the acceleration structure
	vkCreateAccelerationStructureKHR(vk::context().device, &accel, nullptr, &result_accel.accel);
	return result_accel;
}

static void cmd_create_blas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
							std::vector<BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress,
							VkQueryPool queryPool) {
	if (queryPool)	// For querying the compaction size
		vkResetQueryPool(vk::context().device, queryPool, 0, static_cast<uint32_t>(indices.size()));
	uint32_t query_cnt{0};
	for (const auto& idx : indices) {
		// Actual allocation of buffer and acceleration structure.
		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = buildAs[idx].size_info.accelerationStructureSize;	 // Will be used to allocate memory.
		buildAs[idx].as = create_acceleration(createInfo);
		// BuildInfo #2 part
		buildAs[idx].build_info.dstAccelerationStructure = buildAs[idx].as.accel;  // Setting where the build lands
		buildAs[idx].build_info.scratchData.deviceAddress =
			scratchAddress;	 // All build are using the same scratch buffer
		// Building the bottom-level-acceleration-structure
		vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[idx].build_info, &buildAs[idx].range_info);

		// Since the scratch buffer is reused across builds, we need a barrier
		// to ensure one build is finished before starting the next one.
		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
							 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
							 nullptr);

		if (queryPool) {
			// Add a query to find the 'real' amount of memory needed, use for
			// compaction
			vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &buildAs[idx].build_info.dstAccelerationStructure,
														  VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
														  queryPool, query_cnt++);
		}
	}
}

static void cmd_compact_blas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
							 std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool) {
	uint32_t query_cnt{0};
	std::vector<BVH> cleanupAS;	 // previous AS to destroy

	// Get the compacted size result back
	std::vector<VkDeviceSize> compact_sizes(static_cast<uint32_t>(indices.size()));
	vkGetQueryPoolResults(vk::context().device, queryPool, 0, (uint32_t)compact_sizes.size(),
						  compact_sizes.size() * sizeof(VkDeviceSize), compact_sizes.data(), sizeof(VkDeviceSize),
						  VK_QUERY_RESULT_WAIT_BIT);

	for (auto idx : indices) {
		buildAs[idx].cleanup_as = buildAs[idx].as;										// previous AS to destroy
		buildAs[idx].size_info.accelerationStructureSize = compact_sizes[query_cnt++];	// new reduced size
		// Creating a compact version of the AS
		VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		asCreateInfo.size = buildAs[idx].size_info.accelerationStructureSize;
		asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildAs[idx].as = create_acceleration(asCreateInfo);
		// Copy the original BLAS to a compact version
		VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
		copyInfo.src = buildAs[idx].build_info.dstAccelerationStructure;
		copyInfo.dst = buildAs[idx].as.accel;
		copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
		vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
	}
}

static void cmd_create_tlas(BVH& tlas, VkCommandBuffer cmdBuf, uint32_t countInstance, vk::Buffer** scratch_buffer,
							VkDeviceAddress inst_buffer_addr, VkBuildAccelerationStructureFlagsKHR flags, bool update) {
	// Wraps a device pointer to the above uploaded instances.
	VkAccelerationStructureGeometryInstancesDataKHR instances_vk{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
	instances_vk.data.deviceAddress = inst_buffer_addr;

	// Put the above into a VkAccelerationStructureGeometryKHR. We need to put
	// the instances struct in a union and label it as instance data.
	VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	topASGeometry.geometry.instances = instances_vk;

	// Find sizes
	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	build_info.flags = flags;
	build_info.geometryCount = 1;
	build_info.pGeometries = &topASGeometry;
	build_info.mode =
		update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	build_info.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	vkGetAccelerationStructureBuildSizesKHR(vk::context().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
											&build_info, &countInstance, &size_info);

#ifdef VK_NV_ray_tracing_motion_blur
	VkAccelerationStructureMotionInfoNV motionInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV};
	motionInfo.maxInstances = countInstance;
#endif

	// Create TLAS
	if (update == false) {
		VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		create_info.size = size_info.accelerationStructureSize;
		tlas = create_acceleration(create_info);
	}

	// Allocate the scratch memory
	*scratch_buffer = drm::get({
		.name = "TLAS Scratch Buffer",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BufferType::STAGING,
		.size = size_info.buildScratchSize,
	});
	// Update build information
	build_info.srcAccelerationStructure = update ? tlas.accel : VK_NULL_HANDLE;
	build_info.dstAccelerationStructure = tlas.accel;
	build_info.scratchData.deviceAddress = (*scratch_buffer)->get_device_address();

	// Build Offsets info: n instances
	VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{countInstance, 0, 0, 0};
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

	// Build the TLAS
	vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &build_info, &pBuildOffsetInfo);
}

//--------------------------------------------------------------------------------------------------
// Create all the BLAS from the vector of BlasInput
// - There will be one BLAS per input-vector entry
// - There will be as many BLAS as input.size()
// - The resulting BLAS (along with the inputs used to build) are stored in
// m_blas,
//   and can be referenced by index.
// - if flag has the 'Compact' flag, the BLAS will be compacted
//
void build_blas(std::vector<BVH>& blases, const std::vector<BlasInput>& input,
				VkBuildAccelerationStructureFlagsKHR flags) {
	uint32_t nb_blas = static_cast<uint32_t>(input.size());
	VkDeviceSize as_total_size{0};	   // Memory size of all allocated BLAS
	uint32_t nb_compactions{0};		   // Nb of BLAS requesting compaction
	VkDeviceSize max_scratch_size{0};  // Largest scratch size

	// Preparing the information for the acceleration build commands.
	std::vector<BuildAccelerationStructure> buildAs(nb_blas);
	for (uint32_t idx = 0; idx < nb_blas; idx++) {
		// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for
		// querying the build sizes. Other information will be filled in the
		// createBlas (see #2)
		buildAs[idx].build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildAs[idx].build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildAs[idx].build_info.flags = input[idx].flags | flags;
		buildAs[idx].build_info.geometryCount = static_cast<uint32_t>(input[idx].as_geom.size());
		buildAs[idx].build_info.pGeometries = input[idx].as_geom.data();

		// Build range information
		buildAs[idx].range_info = input[idx].as_build_offset_info.data();

		// Finding sizes to create acceleration structures and scratch
		std::vector<uint32_t> maxPrimCount(input[idx].as_build_offset_info.size());
		for (auto tt = 0; tt < input[idx].as_build_offset_info.size(); tt++) {
			maxPrimCount[tt] = input[idx].as_build_offset_info[tt].primitiveCount;	// Number of primitives/triangles
		}
		vkGetAccelerationStructureBuildSizesKHR(context().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&buildAs[idx].build_info, maxPrimCount.data(), &buildAs[idx].size_info);

		// Extra info
		as_total_size += buildAs[idx].size_info.accelerationStructureSize;
		max_scratch_size = std::max(max_scratch_size, buildAs[idx].size_info.buildScratchSize);
		nb_compactions +=
			has_flag(buildAs[idx].build_info.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
	}

	// Allocate the scratch buffers holding the temporary data of the
	// acceleration structure builder
	vk::Buffer* scratch_buffer = drm::get({
		.name = "BLAS Scratch Buffer",
		.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = max_scratch_size,
	});

	// Allocate a query pool for storing the needed size for every BLAS
	// compaction.
	VkQueryPool queryPool{VK_NULL_HANDLE};
	if (nb_compactions > 0)	 // Is compaction requested?
	{
		assert(nb_compactions == nb_blas);	// Don't allow mix of on/off compaction
		VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		qpci.queryCount = nb_blas;
		qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		vkCreateQueryPool(context().device, &qpci, nullptr, &queryPool);
	}
	// Batching creation/compaction of BLAS to allow staying in restricted
	// amount of memory
	std::vector<uint32_t> indices;	// Indices of the BLAS to create
	VkDeviceSize batchSize{0};
	VkDeviceSize batchLimit{256'000'000};  // 256 MB
	for (uint32_t idx = 0; idx < nb_blas; idx++) {
		indices.push_back(idx);
		batchSize += buildAs[idx].size_info.accelerationStructureSize;
		// Over the limit or last BLAS element
		if (batchSize >= batchLimit || idx == nb_blas - 1) {
			vk::CommandBuffer cmdBuf(true, 0, QueueType::GFX);
			cmd_create_blas(cmdBuf.handle, indices, buildAs, scratch_buffer->get_device_address(), queryPool);
			cmdBuf.submit();
			if (queryPool) {
				cmd_compact_blas(cmdBuf.handle, indices, buildAs, queryPool);
				cmdBuf.submit();
				// Destroy the non-compacted version
				for (auto i : indices) {
					vkDestroyAccelerationStructureKHR(context().device, buildAs[i].cleanup_as.accel, nullptr);
					prm::remove(buildAs[i].cleanup_as.buffer);
				}
			}
			// Reset
			batchSize = 0;
			indices.clear();
		}
	}

	// Logging reduction
	if (queryPool) {
		VkDeviceSize compact_size =
			std::accumulate(buildAs.begin(), buildAs.end(), 0ULL,
							[](const auto& a, const auto& b) { return a + b.size_info.accelerationStructureSize; });
		LUMEN_TRACE(" RT BLAS: reducing from: %u to: %u = %u (%2.2f%s smaller) \n", as_total_size, compact_size,
					as_total_size - compact_size, (as_total_size - compact_size) / float(as_total_size) * 100.f, "%");
	}

	// Keeping all the created acceleration structures
	for (auto& b : buildAs) {
		blases.emplace_back(b.as);
	}
	// Clean up
	vkDestroyQueryPool(context().device, queryPool, nullptr);
	drm::destroy(scratch_buffer);
}

// Build TLAS from an array of VkAccelerationStructureInstanceKHR
// - Use motion=true with VkAccelerationStructureMotionInstanceNV
// - The resulting TLAS will be stored in m_tlas
// - update is to rebuild the Tlas with updated matrices, flag must have the
// 'allow_update'
void build_tlas(BVH& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances,
				VkBuildAccelerationStructureFlagsKHR flags, bool update) {
	// Cannot call buildTlas twice except to update.
	uint32_t count_instance = static_cast<uint32_t>(instances.size());

	// Command buffer to create the TLAS

	// Create a buffer holding the actual instance data (matrices++) for use by
	// the AS builder
	vk::Buffer* instances_buf = drm::get({
		.name = "TLAS Instances Buffer",
		.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		.memory_type = vk::BufferType::GPU,
		.size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
		.data = instances.data(),
	});
	vk::CommandBuffer cmd(true, 0, QueueType::GFX);
	// Make sure the copy of the instance buffer are copied before triggering
	// the acceleration structure build
	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
						 nullptr);

	vk::Buffer* scratch_buffer;
	// Creating the TLAS
	cmd_create_tlas(tlas, cmd.handle, count_instance, &scratch_buffer, instances_buf->get_device_address(), flags,
					update);

	// Finalizing and destroying temporary data
	cmd.submit();
	drm::destroy(instances_buf);
	drm::destroy(scratch_buffer);
	tlas.updated = true;
}

}  // namespace vk
