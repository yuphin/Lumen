
#include "AccelerationStructure.h"
#include "VkUtils.h"
#include "PersistentResourceManager.h"
#include "DynamicResourceManager.h"
#include "Framework/CommandBuffer.h"

namespace vk {

static constexpr u64 BATCH_LIMIT = 256'000'000;	 // 256 MB
static constexpr u32 MAX_BLAS_BATCH_SIZE = 256;	 // Max number of BLAS to build/compact in one go

struct BuildAccelerationStructure {
	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	const VkAccelerationStructureBuildRangeInfoKHR* range_info;
	BVH* as;  // result acceleration structure
	BVH cleanup_as;
};

inline static bool has_flag(VkFlags item, VkFlags flag) { return (item & flag) == flag; }

static BVH create_acceleration(VkAccelerationStructureCreateInfoKHR& accel, lm::String name) {
	BVH result_accel;
	// TODO: Potential synchronization issue here if multiple threads contend
	result_accel.buffer = prm::get_buffer(
		{.name = name,
		 .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		 .memory_type = vk::BUFFER_TYPE_GPU,
		 .size = accel.size,
		 .dedicated_allocation = true});
	accel.buffer = result_accel.buffer->handle;
	vkCreateAccelerationStructureKHR(vk::context().device, &accel, nullptr, &result_accel.accel);
	return result_accel;
}

void BVH::destroy() {
	// Destroying AS implies having to update AS descriptors in the render graph with the new AS
	if (accel) {
		// vk::render_graph()->update_as_descriptors(*this);
		vkDestroyAccelerationStructureKHR(vk::context().device, accel, nullptr);
		accel = VK_NULL_HANDLE;
	}
	if (buffer) {
		prm::remove(buffer);
		buffer = nullptr;
	}
}

static void cmd_create_blas(VkCommandBuffer cmd_buf, util::Slice<u32> indices,
							lm::FixedArray<BuildAccelerationStructure>& build_as, VkDeviceAddress scratchAddress,
							VkQueryPool query_pool) {
	if (query_pool) {
		vkResetQueryPool(vk::context().device, query_pool, 0, static_cast<u32>(indices.size));
	}
	u32 query_cnt{0};
	for (u32 idx : indices) {
		// Actual allocation of buffer and acceleration structure.
		VkAccelerationStructureCreateInfoKHR as_ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		as_ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		as_ci.size = build_as[idx].size_info.accelerationStructureSize;	 // Will be used to allocate memory.
		if (!build_as[idx].as->accel) {
			*build_as[idx].as = create_acceleration(as_ci, CSTR("BLAS buffer"));
		}
		// BuildInfo #2 part
		build_as[idx].build_info.dstAccelerationStructure = build_as[idx].as->accel;  // Setting where the build lands
		build_as[idx].build_info.scratchData.deviceAddress =
			scratchAddress;	 // All build are using the same scratch buffer
		// Building the bottom-level-acceleration-structure
		vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &build_as[idx].build_info, &build_as[idx].range_info);

		// Since the scratch buffer is reused across builds, we need a barrier
		// to ensure one build is finished before starting the next one.
		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
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

static void cmd_compact_blas(VkCommandBuffer cmd_buf, util::Slice<u32> indices,
							 lm::FixedArray<BuildAccelerationStructure>& build_as, VkQueryPool query_pool) {
	u32 query_cnt = 0;

	// Get the compacted size result back
	lm::SmallArray<VkDeviceSize, MAX_BLAS_BATCH_SIZE> compact_sizes;
	compact_sizes.resize(indices.size);

	vkGetQueryPoolResults(vk::context().device, query_pool, 0, (u32)compact_sizes.size,
						  compact_sizes.size * sizeof(VkDeviceSize), compact_sizes.data, sizeof(VkDeviceSize),
						  VK_QUERY_RESULT_WAIT_BIT);

	for (u32 idx : indices) {
		build_as[idx].cleanup_as = *build_as[idx].as;									 // previous AS to destroy
		build_as[idx].size_info.accelerationStructureSize = compact_sizes[query_cnt++];	 // new reduced size
		// Creating a compact version of the AS
		VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		asCreateInfo.size = build_as[idx].size_info.accelerationStructureSize;
		asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		*build_as[idx].as = create_acceleration(asCreateInfo, CSTR("BLAS compact buffer"));

		LUMEN_ASSERT(build_as[idx].as->accel != build_as[idx].cleanup_as.accel,
					 "BLAS compacted AS is the same as the original AS");
		// Copy the original BLAS to a compact version
		VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
		copyInfo.src = build_as[idx].build_info.dstAccelerationStructure;
		copyInfo.dst = build_as[idx].as->accel;
		copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
		vkCmdCopyAccelerationStructureKHR(cmd_buf, &copyInfo);
	}
}

static void cmd_create_tlas(BVH& tlas, VkCommandBuffer cmd_buf, u32 primitive_count, vk::Buffer** scratch_buffer_ref,
							bool export_scratch_buffer, VkDeviceAddress inst_buffer_addr,
							VkBuildAccelerationStructureFlagsKHR flags, bool update) {
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
	if (update == false && tlas.accel == VK_NULL_HANDLE) {
		VkAccelerationStructureCreateInfoKHR create_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		create_info.size = size_info.accelerationStructureSize;
		tlas = create_acceleration(create_info, CSTR("TLAS buffer"));
	}

	if (!(export_scratch_buffer && *scratch_buffer_ref && (*scratch_buffer_ref)->size >= size_info.buildScratchSize)) {
		if (export_scratch_buffer && *scratch_buffer_ref) {
			// Unfortunately this is needed here since multiple commands in flight may contend for the same scratch
			// buffer. On application side this can be mitigated by triple buffering the scratch buffer But this is not
			// always feasible.
			LUMEN_WARN("TLAS Build: Waiting for device while resizing scratch buffer");
			vkDeviceWaitIdle(context().device);
			drm::destroy(*scratch_buffer_ref);
		}
		*scratch_buffer_ref =
			drm::get({.name = CSTR("TLAS Scratch Buffer"),
					  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					  .memory_type = vk::BUFFER_TYPE_STAGING,
					  .size = size_info.buildScratchSize,
					  .dedicated_allocation = false});
	}

	// Update build information
	build_info.srcAccelerationStructure = update ? tlas.accel : VK_NULL_HANDLE;
	build_info.dstAccelerationStructure = tlas.accel;
	build_info.scratchData.deviceAddress = (*scratch_buffer_ref)->device_address();

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
static void build_blas_impl(lm::ScratchArena& scratch, lm::FixedArray<BuildAccelerationStructure>& build_as,
							util::Slice<BlasInput> input, VkBuildAccelerationStructureFlagsKHR flags,
							VkCommandBuffer external_cmd_buf, vk::Buffer** scratch_buffer_ref) {
	u32 num_blases = static_cast<u32>(input.size);
	VkDeviceSize as_total_size{0};	   // Memory size of all allocated BLAS
	u32 num_compactions{0};			   // Nb of BLAS requesting compaction
	VkDeviceSize max_scratch_size{0};  // Largest scratch size

	// Preparing the information for the acceleration build commands.
	for (u32 idx = 0; idx < num_blases; idx++) {
		// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for
		// querying the build sizes.

		lm::ScratchArena temp_scratch = scratch.arena;
		build_as[idx].build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		build_as[idx].build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		build_as[idx].build_info.flags = input[idx].flags | flags;
		build_as[idx].build_info.geometryCount = static_cast<u32>(input[idx].as_geom.size());
		build_as[idx].build_info.pGeometries = input[idx].as_geom.data();

		// Build range information
		build_as[idx].range_info = input[idx].as_build_offset_info.data();

		// Finding sizes to create acceleration structures and scratch
		lm::FixedArray<u32> max_prim_counts =
			lm::fixed_array_create<u32>(temp_scratch.arena, input[idx].as_build_offset_info.size());

		for (u64 tt = 0; tt < input[idx].as_build_offset_info.size(); tt++) {
			max_prim_counts.push_back(
				input[idx].as_build_offset_info[tt].primitiveCount);  // Number of primitives/triangles
		}
		vkGetAccelerationStructureBuildSizesKHR(context().device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&build_as[idx].build_info, max_prim_counts.data,
												&build_as[idx].size_info);

		// Extra info
		as_total_size += build_as[idx].size_info.accelerationStructureSize;
		max_scratch_size = glm::max(max_scratch_size, build_as[idx].size_info.buildScratchSize);
		num_compactions +=
			has_flag(build_as[idx].build_info.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
	}

	// Allocate the scratch buffers holding the temporary data of the
	// acceleration structure builder
	bool scratch_buffer_created = false;
	vk::Buffer* scratch_buffer = nullptr;

	bool export_scratch_buffer = external_cmd_buf != VK_NULL_HANDLE;
	if (export_scratch_buffer && *scratch_buffer_ref && (*scratch_buffer_ref)->size >= max_scratch_size) {
		scratch_buffer = *scratch_buffer_ref;
	} else {
		if (export_scratch_buffer && *scratch_buffer_ref) {
			// Unfortunately this is needed here since multiple commands in flight may contend for the same scratch
			// buffer. On application side this can be mitigated by triple buffering the scratch buffer But this is not
			// always feasible.
			LUMEN_WARN("BLAS Build: Waiting for device while resizing scratch buffer");
			vkDeviceWaitIdle(context().device);
			drm::destroy(*scratch_buffer_ref);
		}
		scratch_buffer_created = true;
		scratch_buffer =
			drm::get({.name = CSTR("BLAS Scratch Buffer"),
					  .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					  .memory_type = vk::BUFFER_TYPE_GPU,
					  .size = max_scratch_size,
					  .dedicated_allocation = false});
		if (export_scratch_buffer) {
			*scratch_buffer_ref = scratch_buffer;
		}
	}

	// Allocate a query pool for storing the needed size for every BLAS
	// compaction.
	VkQueryPool compaction_query_pool = VK_NULL_HANDLE;
	if (num_compactions > 0) {					// Is compaction requested?
		assert(num_compactions == num_blases);	// Don't allow mix of on/off compaction
		assert(external_cmd_buf ==
			   VK_NULL_HANDLE);	 // Compaction require an internal command buffer because of the in-between submission
		VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		qpci.queryCount = num_blases;
		qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		vkCreateQueryPool(context().device, &qpci, nullptr, &compaction_query_pool);
	}
	// Batching creation/compaction of BLAS to allow staying in restricted
	// amount of memory
	lm::SmallArray<u32, MAX_BLAS_BATCH_SIZE> indices;
	VkDeviceSize batch_size = 0;
	VkDeviceSize batch_limit = BATCH_LIMIT;
	for (u32 idx = 0; idx < num_blases; idx++) {
		indices.push_back(idx);
		batch_size += build_as[idx].size_info.accelerationStructureSize;
		// Over the limit or last BLAS element
		if (batch_size >= batch_limit || idx == num_blases - 1 || indices.size == MAX_BLAS_BATCH_SIZE) {
			util::Slice<u32> indices_slice(indices.data, indices.size);
			if (external_cmd_buf) {
				cmd_create_blas(external_cmd_buf, indices_slice, build_as, scratch_buffer->device_address(),
								compaction_query_pool);
			} else {
				vk::CommandBuffer cmd(true);
				cmd_create_blas(cmd.handle, indices_slice, build_as, scratch_buffer->device_address(),
								compaction_query_pool);
				cmd.submit();
				if (compaction_query_pool) {
					cmd.begin();
					cmd_compact_blas(cmd.handle, indices_slice, build_as, compaction_query_pool);
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
		LUMEN_TRACE("RT BLAS: reducing from: %.2f MB to: %.2f MB = (%.2f%% smaller) \n", as_total_size * 1e-6,
					compact_size * 1e-6, (as_total_size - compact_size) / f32(as_total_size) * 100.f);
	}
	// Clean up
	vkDestroyQueryPool(context().device, compaction_query_pool, nullptr);
	if (scratch_buffer_created && !export_scratch_buffer) {
		drm::destroy(scratch_buffer);
	}
}

void blas_build(lm::ScratchArena& scratch, util::Slice<BVH> blases, util::Slice<BlasInput> input,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd_buf, vk::Buffer** scratch_buffer) {
	LUMEN_ASSERT(blases.size == input.size, "Mismatch between input and output sizes");
	auto build_as = lm::fixed_array_create<BuildAccelerationStructure>(scratch.arena, input.size);
	for (u64 i = 0; i < input.size; i++) {
		BuildAccelerationStructure& build_as_entry = build_as.push();
		build_as_entry = {};
		build_as_entry.as = &blases[i];
	}
	build_blas_impl(scratch, build_as, input, flags, cmd_buf, scratch_buffer);
}

// Build TLAS from an array of VkAccelerationStructureInstanceKHR
// - Use motion=true with VkAccelerationStructureMotionInstanceNV
// - The resulting TLAS will be stored in m_tlas
// - update is to rebuild the Tlas with updated matrices, flag must have the
// 'allow_update'
void tlas_build(BVH& tlas, util::Slice<VkAccelerationStructureInstanceKHR> instances,
				VkBuildAccelerationStructureFlagsKHR flags, bool update) {
	vk::Buffer* instances_buf = drm::get({.name = CSTR("TLAS Instances Buffer"),
										  .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
												   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
										  .memory_type = vk::BUFFER_TYPE_GPU,
										  .size = sizeof(VkAccelerationStructureInstanceKHR) * instances.size,
										  .data = instances.data,
										  .dedicated_allocation = true});
	// Make sure the copy of the instance buffer are copied before triggering
	// the acceleration structure build
	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

	vk::Buffer* scratch_buffer;
	vk::CommandBuffer cmd(true, 0, QueueType::GFX);
	vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
						 nullptr);
	// Creating the TLAS
	cmd_create_tlas(tlas, cmd.handle, instances.size, &scratch_buffer, /*export_scratch_buffer=*/false,
					instances_buf->device_address(), flags, update);
	cmd.submit();
	drm::destroy(scratch_buffer);
	drm::destroy(instances_buf);
}

void tlas_build(BVH& tlas, vk::Buffer* instances_buf, u32 instance_count, VkBuildAccelerationStructureFlagsKHR flags,
				VkCommandBuffer cmd_buf, vk::Buffer** scratch_buffer_ref, bool update) {
	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

	vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
						 nullptr);
	// Creating the TLAS
	cmd_create_tlas(tlas, cmd_buf, instance_count, scratch_buffer_ref, /*export_scratch_buffer=*/true,
					instances_buf->device_address(), flags, update);
}

BlasInput to_vk_geometry(u32 vtx_count, u32 idx_count, u32 vtx_offset, u32 first_idx, VkDeviceAddress vertex_address,
						 u64 vertex_stride, VkDeviceAddress index_address) {
	u32 maxPrimitiveCount = idx_count / 3;

	// Describe buffer as array of VertexObj.
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
	triangles.vertexData.deviceAddress = vertex_address;
	triangles.vertexStride = vertex_stride;
	// Describe index data (32-bit unsigned i32)
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = index_address;
	// Indicate identity transform by setting transformData to null device
	// pointer.
	// triangles.transformData = {};
	triangles.maxVertex = vtx_offset + vtx_count - 1;

	// Identify the above data as containing opaque triangles.
	VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;	 // For AnyHit
	asGeom.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR offset;
	offset.firstVertex = vtx_offset;
	offset.primitiveCount = maxPrimitiveCount;
	offset.primitiveOffset = first_idx * sizeof(u32);
	offset.transformOffset = 0;

	// Our blas is made from only one geometry, but could be made of many
	// geometries
	BlasInput input;
	input.as_geom.emplace_back(asGeom);
	input.as_build_offset_info.emplace_back(offset);
	return input;
}

}  // namespace vk
