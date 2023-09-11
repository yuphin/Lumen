#include "../LumenPCH.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "VkUtils.h"

#ifdef _WIN64
#include <VersionHelpers.h>
#include <dxgi1_2.h>
#include <aclapi.h>
class WindowsSecurityAttributes {
   protected:
	SECURITY_ATTRIBUTES m_winSecurityAttributes;
	PSECURITY_DESCRIPTOR m_winPSecurityDescriptor;

   public:
	WindowsSecurityAttributes();
	SECURITY_ATTRIBUTES *operator&();
	~WindowsSecurityAttributes();
};

WindowsSecurityAttributes::WindowsSecurityAttributes() {
	m_winPSecurityDescriptor = (PSECURITY_DESCRIPTOR)calloc(1, SECURITY_DESCRIPTOR_MIN_LENGTH + 2 * sizeof(void **));
	if (!m_winPSecurityDescriptor) {
		throw std::runtime_error("Failed to allocate memory for security descriptor");
	}

	PSID *ppSID = (PSID *)((PBYTE)m_winPSecurityDescriptor + SECURITY_DESCRIPTOR_MIN_LENGTH);
	PACL *ppACL = (PACL *)((PBYTE)ppSID + sizeof(PSID *));

	InitializeSecurityDescriptor(m_winPSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

	SID_IDENTIFIER_AUTHORITY sidIdentifierAuthority = SECURITY_WORLD_SID_AUTHORITY;
	AllocateAndInitializeSid(&sidIdentifierAuthority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, ppSID);

	EXPLICIT_ACCESS explicitAccess;
	ZeroMemory(&explicitAccess, sizeof(EXPLICIT_ACCESS));
	explicitAccess.grfAccessPermissions = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
	explicitAccess.grfAccessMode = SET_ACCESS;
	explicitAccess.grfInheritance = INHERIT_ONLY;
	explicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	explicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	explicitAccess.Trustee.ptstrName = (LPTSTR)*ppSID;

	SetEntriesInAcl(1, &explicitAccess, NULL, ppACL);

	SetSecurityDescriptorDacl(m_winPSecurityDescriptor, TRUE, *ppACL, FALSE);

	m_winSecurityAttributes.nLength = sizeof(m_winSecurityAttributes);
	m_winSecurityAttributes.lpSecurityDescriptor = m_winPSecurityDescriptor;
	m_winSecurityAttributes.bInheritHandle = TRUE;
}

SECURITY_ATTRIBUTES *WindowsSecurityAttributes::operator&() { return &m_winSecurityAttributes; }

WindowsSecurityAttributes::~WindowsSecurityAttributes() {
	PSID *ppSID = (PSID *)((PBYTE)m_winPSecurityDescriptor + SECURITY_DESCRIPTOR_MIN_LENGTH);
	PACL *ppACL = (PACL *)((PBYTE)ppSID + sizeof(PSID *));

	if (*ppSID) {
		FreeSid(*ppSID);
	}
	if (*ppACL) {
		LocalFree(*ppACL);
	}
	free(m_winPSecurityDescriptor);
}
#endif /* _WIN64 */

static VkExternalMemoryHandleTypeFlagBits getDefaultMemHandleType() {
#ifdef _WIN64
	return IsWindows8Point1OrGreater() ? VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
									   : VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
#else
	return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif /* _WIN64 */
}

void Buffer::create(const char* name, VulkanContext* ctx,
					VkBufferUsageFlags usage,
					VkMemoryPropertyFlags mem_property_flags,
					VkSharingMode sharing_mode, VkDeviceSize size, void* data,
					bool use_staging, bool external) {
	if (!this->ctx) {
		this->ctx = ctx;
		this->mem_property_flags = mem_property_flags;
		this->usage_flags = usage;
	}

	if (use_staging) {
		Buffer staging_buffer;

		LUMEN_ASSERT(mem_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					 "Buffer creation error");
		staging_buffer.create(ctx, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
								  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
							  VK_SHARING_MODE_EXCLUSIVE, size, data);

		staging_buffer.unmap();
		this->create(ctx, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
					 mem_property_flags, sharing_mode, size, nullptr);

		CommandBuffer copy_cmd(ctx, true);
		VkBufferCopy copy_region = {};

		copy_region.size = size;
		vkCmdCopyBuffer(copy_cmd.handle, staging_buffer.handle, this->handle, 1,
						&copy_region);
		copy_cmd.submit(ctx->queues[0]);
		staging_buffer.destroy();
	} else {
		// Create the buffer handle
		VkBufferCreateInfo buffer_CI =
			vk::buffer_create_info(usage, size, sharing_mode);

		VkExternalMemoryBufferCreateInfo externalMemoryBufferInfo = {};
		externalMemoryBufferInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
		externalMemoryBufferInfo.handleTypes = getDefaultMemHandleType();
		if (external) {
			buffer_CI.pNext = &externalMemoryBufferInfo;
#ifdef _WIN64
			WindowsSecurityAttributes winSecurityAttributes;

			VkExportMemoryWin32HandleInfoKHR vulkanExportMemoryWin32HandleInfoKHR = {};
			vulkanExportMemoryWin32HandleInfoKHR.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
			vulkanExportMemoryWin32HandleInfoKHR.pNext = NULL;
			vulkanExportMemoryWin32HandleInfoKHR.pAttributes = &winSecurityAttributes;
			vulkanExportMemoryWin32HandleInfoKHR.dwAccess = DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE;
			vulkanExportMemoryWin32HandleInfoKHR.name = (LPCWSTR)NULL;
#endif /* _WIN64 */
			VkExportMemoryAllocateInfoKHR vulkanExportMemoryAllocateInfoKHR = {};
			vulkanExportMemoryAllocateInfoKHR.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
#ifdef _WIN64
			vulkanExportMemoryAllocateInfoKHR.pNext =
				getDefaultMemHandleType() & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
					? &vulkanExportMemoryWin32HandleInfoKHR
					: NULL;
			vulkanExportMemoryAllocateInfoKHR.handleTypes = getDefaultMemHandleType();
#else
			vulkanExportMemoryAllocateInfoKHR.pNext = NULL;
			vulkanExportMemoryAllocateInfoKHR.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif /* _WIN64 */
		}
		vk::check(
			vkCreateBuffer(ctx->device, &buffer_CI, nullptr, &this->handle),
			"Failed to create vertex buffer!");

		// Create the memory backing up the buffer handle
		VkMemoryRequirements mem_reqs;
		VkMemoryAllocateInfo mem_alloc_info = vk::memory_allocate_info();
		vkGetBufferMemoryRequirements(ctx->device, this->handle, &mem_reqs);

		mem_alloc_info.allocationSize = mem_reqs.size;
		// Find a memory type index that fits the properties of the buffer
		mem_alloc_info.memoryTypeIndex = find_memory_type(
			&ctx->physical_device, mem_reqs.memoryTypeBits, mem_property_flags);

		VkMemoryAllocateFlagsInfo flags_info{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
		if (usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			flags_info.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
			mem_alloc_info.pNext = &flags_info;
		}
		vk::check(vkAllocateMemory(ctx->device, &mem_alloc_info, nullptr,
								   &this->buffer_memory),
				  "Failed to allocate buffer memory!");

		alignment = mem_reqs.alignment;
		this->size = size;
		usage_flags = usage;
		if (mem_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			this->map();
		}

		// If a pointer to the buffer data has been passed, map the buffer and
		// copy over the data
		if (data != nullptr) {
			// Memory is assumed mapped at this point
			memcpy(this->data, data, size);
			if ((mem_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ==
				0) {
				this->flush();
			}
		}

		// Initialize a default descriptor that covers the whole buffer size
		this->prepare_descriptor();
		this->bind();
	}
	if (name) {
		DebugMarker::set_resource_name(ctx->device, (uint64_t)handle, name,
									   VK_OBJECT_TYPE_BUFFER);
		this->name = name;
	}
}
void Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
	VkMappedMemoryRange mapped_range = {};
	mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mapped_range.memory = buffer_memory;
	mapped_range.offset = offset;
	mapped_range.size = size;
	vk::check(vkFlushMappedMemoryRanges(ctx->device, 1, &mapped_range),
			  "Failed to flush mapped memory ranges");
}

void Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
	VkMappedMemoryRange mapped_range = {};
	mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mapped_range.memory = buffer_memory;
	mapped_range.offset = offset;
	mapped_range.size = size;
	vk::check(vkInvalidateMappedMemoryRanges(ctx->device, 1, &mapped_range),
			  "Failed to invalidate mapped memory range");
}

void Buffer::prepare_descriptor(VkDeviceSize size, VkDeviceSize offset) {
	descriptor.offset = offset;
	descriptor.buffer = handle;
	descriptor.range = size;
}

void Buffer::copy(Buffer& dst_buffer, VkCommandBuffer cmdbuf) {
	VkBufferCopy copy_region;
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = dst_buffer.size;
	vkCmdCopyBuffer(cmdbuf, handle, dst_buffer.handle, 1, &copy_region);
	VkBufferMemoryBarrier copy_barrier =
		buffer_barrier(dst_buffer.handle, VK_ACCESS_TRANSFER_WRITE_BIT,
					   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &copy_barrier, 0,
						 0);
}
