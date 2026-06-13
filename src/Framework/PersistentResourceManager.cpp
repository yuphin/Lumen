
#include "PersistentResourceManager.h"
#include "Base/HashMap.h"
#include "Base/Memory.h"
#include "Base/OS.h"
#include "VulkanContext.h"
#include "VkUtils.h"

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

static bool sampler_eq(const VkSamplerCreateInfo& lhs, const VkSamplerCreateInfo& rhs) { return lhs == rhs; }

template <typename T>
static void hash_value(u64& hash, const T& value) {
	hash = lm::fnv1a_hash((void*)&value, sizeof(T), hash);
}

static void hash_f32(u64& hash, f32 value) {
	if (value == 0.0f) {
		value = 0.0f;
	}
	hash_value(hash, value);
}

static u64 sampler_hash(const VkSamplerCreateInfo& ci) {
	u64 hash = lm::FNV_64_OFFSET_BIAS;
	u64 pnext = reinterpret_cast<u64>(ci.pNext);
	hash_value(hash, ci.sType);
	hash_value(hash, pnext);
	hash_value(hash, ci.flags);
	hash_value(hash, ci.magFilter);
	hash_value(hash, ci.minFilter);
	hash_value(hash, ci.mipmapMode);
	hash_value(hash, ci.addressModeU);
	hash_value(hash, ci.addressModeV);
	hash_value(hash, ci.addressModeW);
	hash_f32(hash, ci.mipLodBias);
	hash_value(hash, ci.anisotropyEnable);
	hash_f32(hash, ci.maxAnisotropy);
	hash_value(hash, ci.compareEnable);
	hash_value(hash, ci.compareOp);
	hash_f32(hash, ci.minLod);
	hash_f32(hash, ci.maxLod);
	hash_value(hash, ci.borderColor);
	hash_value(hash, ci.unnormalizedCoordinates);
	return hash;
}

template <typename T>
struct PersistentPool {
	PersistentPool(lm::String arena_name) : arena_name(arena_name) {}

	T* get(bool use_mutex) {
		os::ScopedLock lock(mutex, use_mutex);
		if (!arena) {
			arena = lm::arena_create(arena_name, MB(4), MB(1));
			free_list = lm::array_create<T*>(arena, 0, 4096);
		}
		if (!free_list.empty()) {
			T* result = free_list.back();
			--free_list.size;
			memset(result, 0, sizeof(T));
			return result;
		}
		return (T*)arena->allocate(sizeof(T), alignof(T), nullptr, /*zero_initialize=*/true);
	}

	void remove(T* ptr) {
		assert(arena);
		free_list.push_back(ptr);
	}

	void destroy() {
		if (!arena) return;
		lm::arena_destroy(arena);
		arena = nullptr;
		free_list = {};
	}

	lm::String arena_name;
	lm::Arena* arena = nullptr;
	lm::Array<T*> free_list;
	os::Mutex mutex;
};

namespace prm {
PersistentPool<vk::Buffer> _buffer_pool(CSTR("Persistent Buffer Pool Arena"));
PersistentPool<vk::Texture> _texture_pool(CSTR("Persistent Texture Pool Arena"));

using SamplerCache = lm::HashMap<VkSamplerCreateInfo, VkSampler, sampler_hash, sampler_eq>;
lm::Arena* _sampler_cache_arena = nullptr;
SamplerCache _sampler_cache;
os::Mutex _sampler_cache_mutex;

VkSampler get_sampler(const VkSamplerCreateInfo& sampler_create_info, bool use_mutex) {
	os::ScopedLock lock(_sampler_cache_mutex, use_mutex);
	if (!_sampler_cache_arena) {
		_sampler_cache_arena = lm::arena_create(CSTR("Sampler Cache Arena"), MB(1), KB(64));
		_sampler_cache =
			lm::hash_map_create<VkSamplerCreateInfo, VkSampler, sampler_hash, sampler_eq>(_sampler_cache_arena, 128);
	}
	auto entry = _sampler_cache.find(sampler_create_info);
	if (entry) {
		return entry->value;
	}
	VkSampler sampler = VK_NULL_HANDLE;
	vk::check(vkCreateSampler(vk::context().device, &sampler_create_info, nullptr, &sampler),
			  "Could not create a sampler");
	vk::set_resource_name(vk::context().device, (u64)sampler, "Sampler", VK_OBJECT_TYPE_SAMPLER);
	_sampler_cache.insert(sampler_create_info, sampler);
	return sampler;
}
vk::Texture* get_texture(const vk::TextureDesc& texture_desc, bool use_mutex) {
	vk::Texture* texture = _texture_pool.get(use_mutex);
	vk::texture_create(texture, texture_desc);
	return texture;
}
vk::Buffer* get_buffer(const vk::BufferDesc& texture_desc, bool use_mutex) {
	vk::Buffer* buffer = _buffer_pool.get(use_mutex);
	vk::buffer_create(buffer, texture_desc);
	return buffer;
}

void remove(vk::Buffer* buffer) {
	if (!buffer) return;
	vk::buffer_destroy(buffer);
	_buffer_pool.remove(buffer);
}
void remove(vk::Texture* texture) {
	if (!texture) return;
	vk::texture_destroy(texture);
	_texture_pool.remove(texture);
}

void destroy() {
	_buffer_pool.destroy();
	_texture_pool.destroy();
	if (_sampler_cache.initialized()) {
		for (auto& entry : _sampler_cache) {
			vkDestroySampler(vk::context().device, entry.value, nullptr);
		}
		_sampler_cache.clear();
	}
	if (_sampler_cache_arena) {
		lm::arena_destroy(_sampler_cache_arena);
		_sampler_cache_arena = nullptr;
		_sampler_cache = {};
	}
}
}  // namespace prm
