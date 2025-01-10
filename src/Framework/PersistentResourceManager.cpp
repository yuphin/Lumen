#include "../LumenPCH.h"

#include "PersistentResourceManager.h"
#include <vulkan/vulkan_core.h>
#include <winnt.h>
#include <unordered_map>
#include "VulkanContext.h"
#include "VkUtils.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/mman.h>
#include <unistd.h>
#endif
static bool operator==(const VkSamplerCreateInfo& lhs, const VkSamplerCreateInfo& rhs) {
	// Compare the individual members of VkSamplerCreateInfo
	return lhs.sType == rhs.sType && lhs.pNext == rhs.pNext && lhs.flags == rhs.flags &&
		   lhs.magFilter == rhs.magFilter && lhs.minFilter == rhs.minFilter && lhs.mipmapMode == rhs.mipmapMode &&
		   lhs.addressModeU == rhs.addressModeU && lhs.addressModeV == rhs.addressModeV &&
		   lhs.addressModeW == rhs.addressModeW && lhs.mipLodBias == rhs.mipLodBias &&
		   lhs.anisotropyEnable == rhs.anisotropyEnable && lhs.maxAnisotropy == rhs.maxAnisotropy &&
		   lhs.compareEnable == rhs.compareEnable && lhs.compareOp == rhs.compareOp && lhs.minLod == rhs.minLod &&
		   lhs.maxLod == rhs.maxLod && lhs.borderColor == rhs.borderColor &&
		   lhs.unnormalizedCoordinates == rhs.unnormalizedCoordinates;
}

namespace os {
size_t get_page_size() {
#if defined(_WIN32) || defined(_WIN64)
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwPageSize;
#else
	return sysconf(_SC_PAGE_SIZE);
#endif
}
}  // namespace os

constexpr size_t RESERVE_SIZE = 1024ull * 1024 * 1024 * 1024 * 64;	// 64GB
template <typename T>
class PersistentPool {
   public:
	PersistentPool() : PAGE_SIZE(os::get_page_size()) {
#if defined(_WIN32) || defined(_WIN64)
		data_base = (T*)VirtualAlloc(NULL, RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
		data_base = (T*)VirtualAlloc(data_base, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
#else
		data_base = (T*)mmap(NULL, RESERVE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		int result = mprotect(data_base, ALLOC_SIZE, PROT_READ | PROT_WRITE)
			LUMEN_ASSERT(result == 0, "Could not allocate memory");
#endif
		base_ptr = data_base;
		next_page = (uint8_t*)data_base + PAGE_SIZE;
	}
	T* get(bool use_mutex) {
		std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
		if (use_mutex) {
			lock.lock();
		}
		if (!free_list.empty()) {
			size_t idx = free_list.back();
			free_list.pop_back();
			return base_ptr + idx;
		}
		while ((uint8_t*)next_page - (uint8_t*)data_base < sizeof(T)) {
#if defined(_WIN32) || defined(_WIN64)
			data_base = (T*)VirtualAlloc(next_page, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
			next_page = (uint8_t*)data_base + PAGE_SIZE;
#else
			int result = mprotect(next_page, PAGE_SIZE, PROT_READ | PROT_WRITE);
			LUMEN_ASSERT(result == 0, "Could not allocate memory");
			next_page = (uint8_t*)next_page + PAGE_SIZE;
#endif
		}
		return data_base++;
	}

	void remove(T* ptr) {
		size_t idx = ptr - base_ptr;
		free_list.push_back(idx);
	}

	void destroy() {
		VirtualFree(base_ptr, 0, MEM_RELEASE);
		data_base = nullptr;
		next_page = nullptr;
		base_ptr = nullptr;
	}

   private:
	const size_t PAGE_SIZE;
	T* base_ptr = nullptr;
	T* data_base = nullptr;
	void* next_page = nullptr;
	std::vector<size_t> free_list;
	std::mutex mutex;
};

namespace prm {
PersistentPool<vk::Buffer> _buffer_pool;
PersistentPool<vk::Texture> _texture_pool;

std::unordered_map<VkSamplerCreateInfo, VkSampler, vk::SamplerHash> _sampler_cache;
std::mutex _sampler_cache_mutex;

VkSampler get_sampler(const VkSamplerCreateInfo& sampler_create_info, bool use_mutex) {
	std::unique_lock<std::mutex> lock(_sampler_cache_mutex, std::defer_lock);
	if (use_mutex) {
		lock.lock();
	}
	auto it = _sampler_cache.find(sampler_create_info);
	if (it != _sampler_cache.end()) {
		return it->second;
	}
	auto result = _sampler_cache.insert({sampler_create_info, VkSampler()});
	if (!result.second) {
		return VK_NULL_HANDLE;
	}
	lock.unlock();
	vk::check(vkCreateSampler(vk::context().device, &sampler_create_info, nullptr, &result.first->second),
			  "Could not create a sampler");
	vk::DebugMarker::set_resource_name(vk::context().device, (uint64_t)result.first->second, "Sampler",
									   VK_OBJECT_TYPE_SAMPLER);
	return result.first->second;
}
vk::Texture* get_texture(const vk::TextureDesc& texture_desc, bool use_mutex) {
	vk::Texture* texture = _texture_pool.get(use_mutex);
	vk::create_texture(texture, texture_desc);
	return texture;
}
vk::Buffer* get_buffer(const vk::BufferDesc& texture_desc, bool use_mutex) {
	vk::Buffer* buffer = _buffer_pool.get(use_mutex);
	memset(buffer, 0, sizeof(vk::Buffer));
	vk::create_buffer(buffer, texture_desc);
	return buffer;
}

void remove(vk::Buffer* buffer) {
	if (!buffer) return;
	vk::destroy_buffer(buffer);
	_buffer_pool.remove(buffer);
	buffer = nullptr;
}
void remove(vk::Texture* texture) {
	if (!texture) return;
	vk::destroy_texture(texture);
	_texture_pool.remove(texture);
	texture = nullptr;
}

void destroy() {
	_buffer_pool.destroy();
	_texture_pool.destroy();
	for (auto& [_, sampler] : _sampler_cache) {
		vkDestroySampler(vk::context().device, sampler, nullptr);
	}
	_sampler_cache.clear();
}
}  // namespace prm