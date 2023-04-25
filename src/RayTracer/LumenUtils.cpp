#include "LumenPCH.h"
#include "LumenUtils.h"
BlasInput to_vk_geometry(LumenPrimMesh& prim, VkDeviceAddress vertex_address, VkDeviceAddress index_address) {
	uint32_t maxPrimitiveCount = prim.idx_count / 3;

	// Describe buffer as array of VertexObj.
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
	triangles.vertexData.deviceAddress = vertex_address;
	triangles.vertexStride = sizeof(glm::vec3);
	// Describe index data (32-bit unsigned int)
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = index_address;
	// Indicate identity transform by setting transformData to null device
	// pointer.
	// triangles.transformData = {};
	triangles.maxVertex = prim.vtx_count;

	// Identify the above data as containing opaque triangles.
	VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;	 // For AnyHit
	asGeom.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR offset;
	offset.firstVertex = prim.vtx_offset;
	offset.primitiveCount = maxPrimitiveCount;
	offset.primitiveOffset = prim.first_idx * sizeof(uint32_t);
	offset.transformOffset = 0;

	// Our blas is made from only one geometry, but could be made of many
	// geometries
	BlasInput input;
	input.as_geom.emplace_back(asGeom);
	input.as_build_offset_info.emplace_back(offset);

	return input;
}