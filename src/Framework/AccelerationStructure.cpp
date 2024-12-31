
#include "LumenPCH.h"
#include "AccelerationStructure.h"
#include "VkUtils.h"
#include "PersistentResourceManager.h"
#include "DynamicResourceManager.h"

namespace vk {
static constexpr size_t BATCH_LIMIT = 256'000'000;	// 256 MB
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
	// TODO: Potential synchronization issue here if multiple threads contend
	result_accel.buffer = prm::get_buffer(
		{.name = "Blas Buffer",
		 .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		 .memory_type = vk::BufferType::GPU,
		 .size = accel.size,
		 .dedicated_allocation = false});
	accel.buffer = result_accel.buffer->handle;
	vkCreateAccelerationStructureKHR(vk::context().device, &accel, nullptr, &result_accel.accel);
	return result_accel;
}

static void cmd_create_blas(VkCommandBuffer cmd_buf, std::vector<uint32_t> indices,
							std::vector<BuildAccelerationStructure>& build_as, VkDeviceAddress scratchAddress,
							VkQueryPool query_pool) {
	if (query_pool) {
		vkResetQueryPool(vk::context().device, query_pool, 0, static_cast<uint32_t>(indices.size()));
	}
	uint32_t query_cnt{0};
	for (const auto& idx : indices) {
		// Actual allocation of buffer and acceleration structure.
		VkAccelerationStructureCreateInfoKHR as_ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		as_ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		as_ci.size = build_as[idx].size_info.accelerationStructureSize;	 // Will be used to allocate memory.
		build_as[idx].as = create_acceleration(as_ci);
		// BuildInfo #2 part
		build_as[idx].build_info.dstAccelerationStructure = build_as[idx].as.accel;	 // Setting where the build lands
		build_as[idx].build_info.scratchData.deviceAddress =
			scratchAddress;	 // All build are using the same scratch buffer
		// Building the bottom-level-acceleration-structure
		vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &build_as[idx].build_info, &build_as[idx].range_info);

		// Since the scratch buffer is reused across builds, we need a barrier
		// to ensure one build is finished before starting the next one.
		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
							 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
							 nullptr);

		if (query_pool) {
			// Add a query to find the 'real' amount of memory needed, use for
			// compaction
			vkCmdWriteAccelerationStructuresPropertiesKHR(
				cmd_buf, 1, &build_as[idx].build_info.dstAccelerationStructure,
				VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, query_pool, query_cnt++);
		}
	}
}

static void cmd_compact_blas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
							 std::vector<BuildAccelerationStructure>& build_as, VkQueryPool query_pool) {
	uint32_t query_cnt{0};
	std::vector<BVH> cleanupAS;	 // previous AS to destroy

	// Get the compacted size result back
	std::vector<VkDeviceSize> compact_sizes(static_cast<uint32_t>(indices.size()));
	vkGetQueryPoolResults(vk::context().device, query_pool, 0, (uint32_t)compact_sizes.size(),
						  compact_sizes.size() * sizeof(VkDeviceSize), compact_sizes.data(), sizeof(VkDeviceSize),
						  VK_QUERY_RESULT_WAIT_BIT);

	for (auto idx : indices) {
		build_as[idx].cleanup_as = build_as[idx].as;									 // previous AS to destroy
		build_as[idx].size_info.accelerationStructureSize = compact_sizes[query_cnt++];	 // new reduced size
		// Creating a compact version of the AS
		VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		asCreateInfo.size = build_as[idx].size_info.accelerationStructureSize;
		asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		build_as[idx].as = create_acceleration(asCreateInfo);
		// Copy the original BLAS to a compact version
		VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
		copyInfo.src = build_as[idx].build_info.dstAccelerationStructure;
		copyInfo.dst = build_as[idx].as.accel;
		copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
		vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
	}
}

static void cmd_create_tlas(BVH& tlas, VkCommandBuffer cmd_buf, uint32_t primitive_count, vk::Buffer** scratch_buffer,
							VkDeviceAddress inst_buffer_addr, VkBuildAccelerationStructureFlagsKHR flags, bool update) {
	// Wraps a device pointer to the above uploaded instances.
	VkAccelerationStructureGeometryInstancesDataKHR instances_vk{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
	instances_vk.data.deviceAddress = inst_buffer_addr;

	// Put the above into a VkAccelerationStructureGeometryKHR. We need to put
	// the instances struct in a union and label it as instance data.
	VkAccelerationStructureGeometryKHR top_as_geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	top_as_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	top_as_geometry.geometry.instances = instances_vk;

	// Find sizes
	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	build_info.flags = flags;
	build_info.geometryCount = 1;
	build_info.pGeometries = &top_as_geometry;
	build_info.mode =
		update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	build_info.srcAccelerationStructure = VK_NULL_HANDLE;

	VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	vkGetAccelerationStructureBuildSizesKHR(vk::context().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
											&build_info, &primitive_count, &size_info);

	// Create TLAS
	if (update == false) {
		VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		create_info.size = size_info.accelerationStructureSize;
		tlas = create_acceleration(create_info);
	}

	// Allocate the scratch memory
	*scratch_buffer = drm::get({.name = "TLAS Scratch Buffer",
								.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								.memory_type = vk::BufferType::STAGING,
								.size = size_info.buildScratchSize,
								.dedicated_allocation = false});
	// Update build information
	build_info.srcAccelerationStructure = update ? tlas.accel : VK_NULL_HANDLE;
	build_info.dstAccelerationStructure = tlas.accel;
	build_info.scratchData.deviceAddress = (*scratch_buffer)->get_device_address();

	// Build Offsets info: n instances
	VkAccelerationStructureBuildRangeInfoKHR build_offset_info{primitive_count, 0, 0, 0};
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &build_offset_info;

	// Build the TLAS
	vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &build_info, &pBuildOffsetInfo);
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

// Existence of cmd_buf implies that cmd_buf handles submission outside of this function
static std::vector<BuildAccelerationStructure> build_blas_impl(const std::vector<BlasInput>& input,
															   VkBuildAccelerationStructureFlagsKHR flags,
															   VkCommandBuffer external_cmd_buf,
															   vk::Buffer** scratch_buffer_ref) {
	uint32_t nb_blas = static_cast<uint32_t>(input.size());
	VkDeviceSize as_total_size{0};	   // Memory size of all allocated BLAS
	uint32_t nb_compactions{0};		   // Nb of BLAS requesting compaction
	VkDeviceSize max_scratch_size{0};  // Largest scratch size

	// Preparing the information for the acceleration build commands.
	std::vector<BuildAccelerationStructure> build_as(nb_blas);
	for (uint32_t idx = 0; idx < nb_blas; idx++) {
		// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for
		// querying the build sizes. Other information will be filled in the
		// createBlas (see #2)
		build_as[idx].build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		build_as[idx].build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_as[idx].build_info.flags = input[idx].flags | flags;
		build_as[idx].build_info.geometryCount = static_cast<uint32_t>(input[idx].as_geom.size());
		build_as[idx].build_info.pGeometries = input[idx].as_geom.data();

		// Build range information
		build_as[idx].range_info = input[idx].as_build_offset_info.data();

		// Finding sizes to create acceleration structures and scratch
		std::vector<uint32_t> maxPrimCount(input[idx].as_build_offset_info.size());
		for (auto tt = 0; tt < input[idx].as_build_offset_info.size(); tt++) {
			maxPrimCount[tt] = input[idx].as_build_offset_info[tt].primitiveCount;	// Number of primitives/triangles
		}
		vkGetAccelerationStructureBuildSizesKHR(context().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&build_as[idx].build_info, maxPrimCount.data(),
												&build_as[idx].size_info);

		// Extra info
		as_total_size += build_as[idx].size_info.accelerationStructureSize;
		max_scratch_size = std::max(max_scratch_size, build_as[idx].size_info.buildScratchSize);
		nb_compactions +=
			has_flag(build_as[idx].build_info.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
	}

	// Allocate the scratch buffers holding the temporary data of the
	// acceleration structure builder
	bool scratch_buffer_created = false;
	vk::Buffer* scratch_buffer = nullptr;
	if (scratch_buffer_ref) {
		scratch_buffer = *scratch_buffer_ref;
	} else {
		scratch_buffer_created = true;
		scratch_buffer =
			drm::get({.name = "BLAS Scratch Buffer",
					  .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					  .memory_type = vk::BufferType::GPU,
					  .size = max_scratch_size,
					  .dedicated_allocation = false});
	}

	// Allocate a query pool for storing the needed size for every BLAS
	// compaction.
	VkQueryPool compaction_query_pool = VK_NULL_HANDLE;
	if (nb_compactions > 0) {				// Is compaction requested?
		assert(nb_compactions == nb_blas);	// Don't allow mix of on/off compaction
		assert(external_cmd_buf ==
			   VK_NULL_HANDLE);	 // Compaction require an internal command buffer because of the in-between submission
		VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		qpci.queryCount = nb_blas;
		qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		vkCreateQueryPool(context().device, &qpci, nullptr, &compaction_query_pool);
	}
	// Batching creation/compaction of BLAS to allow staying in restricted
	// amount of memory
	std::vector<uint32_t> indices;	// Indices of the BLAS to create
	VkDeviceSize batch_size{0};
	VkDeviceSize batch_limit{BATCH_LIMIT};
	for (uint32_t idx = 0; idx < nb_blas; idx++) {
		indices.push_back(idx);
		batch_size += build_as[idx].size_info.accelerationStructureSize;
		// Over the limit or last BLAS element
		if (batch_size >= batch_limit || idx == nb_blas - 1) {
			if (external_cmd_buf) {
				cmd_create_blas(external_cmd_buf, indices, build_as, scratch_buffer->get_device_address(),
								compaction_query_pool);
			} else {
				vk::CommandBuffer cmd(true);
				cmd_create_blas(cmd.handle, indices, build_as, scratch_buffer->get_device_address(),
								compaction_query_pool);
				cmd.submit();
				if (compaction_query_pool) {
					cmd.begin();
					cmd_compact_blas(cmd.handle, indices, build_as, compaction_query_pool);
					cmd.submit();
					// Destroy the non-compacted version
					for (auto i : indices) {
						vkDestroyAccelerationStructureKHR(context().device, build_as[i].cleanup_as.accel, nullptr);
						prm::remove(build_as[i].cleanup_as.buffer);
					}
				}
			}
			// Reset
			batch_size = 0;
			indices.clear();
		}
	}

	// Logging reduction
	if (compaction_query_pool) {
		VkDeviceSize compact_size =
			std::accumulate(build_as.begin(), build_as.end(), 0ULL,
							[](const auto& a, const auto& b) { return a + b.size_info.accelerationStructureSize; });
		LUMEN_TRACE("RT BLAS: reducing from: {} MB to: {} MB = ({}% smaller) \n", as_total_size * 1e-6,
					compact_size * 1e-6, (as_total_size - compact_size) / float(as_total_size) * 100.f);
	} else {
		LUMEN_TRACE("RT BLAS: total size: {} MB\n", as_total_size * 1e-6);
	}
	// Clean up
	vkDestroyQueryPool(context().device, compaction_query_pool, nullptr);
	if (scratch_buffer_created) {
		drm::destroy(scratch_buffer);
	}
	return build_as;
}

void build_blas(std::vector<BVH>& blases, const std::vector<BlasInput>& input,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd_buf, vk::Buffer** scratch_buffer) {
	std::vector<BuildAccelerationStructure> build_as = build_blas_impl(input, flags, cmd_buf, scratch_buffer);
	for (auto& ba : build_as) {
		blases.emplace_back(ba.as);
	}
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
	vk::Buffer* instances_buf = drm::get({.name = "TLAS Instances Buffer",
										  .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
												   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
										  .memory_type = vk::BufferType::GPU,
										  .size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
										  .data = instances.data(),
										  .dedicated_allocation = false});
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
	vk::render_graph()->set_pipelines_dirty(true, false);
}

}  // namespace vk
