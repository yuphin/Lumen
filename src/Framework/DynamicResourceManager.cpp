
#include "Buffer.h"
#include "Texture.h"
#include "DynamicResourceManager.h"
#include "Base/Memory.h"

static constexpr u64 MAX_DYNAMIC_RESOURCES = 512;
static constexpr u64 DYNAMIC_POOL_WORD_BITS = 64;
static constexpr u64 DYNAMIC_POOL_WORD_COUNT = MAX_DYNAMIC_RESOURCES / DYNAMIC_POOL_WORD_BITS;

static u64 first_unset_bit(u64 mask) {
	for (u64 bit = 0; bit < DYNAMIC_POOL_WORD_BITS; ++bit) {
		if ((mask & (u64(1) << bit)) == 0) {
			return bit;
		}
	}
	return DYNAMIC_POOL_WORD_BITS;
}

template <typename T>
struct DynamicPool {
	DynamicPool(lm::String arena_name) : arena_name(arena_name) {}

	T* get() {
		if (!arena) {
			arena = lm::arena_create(arena_name, MB(1), KB(64));
			data = lm::fixed_array_create<T>(arena, MAX_DYNAMIC_RESOURCES);
		}
		for (u64 word_idx = 0; word_idx < DYNAMIC_POOL_WORD_COUNT; ++word_idx) {
			u64 bit_idx = first_unset_bit(availability_mask[word_idx]);
			if (bit_idx < DYNAMIC_POOL_WORD_BITS) {
				u64 idx = word_idx * DYNAMIC_POOL_WORD_BITS + bit_idx;
				availability_mask[word_idx] |= u64(1) << bit_idx;
				T* result = &data.data[idx];
				memset(result, 0, sizeof(T));
				return result;
			}
		}
		LUMEN_ASSERT(false, "Dynamic resource pool exhausted");
		return nullptr;
	}

	void remove(T* object) {
		assert(arena);
		u64 idx = object - data.data;
		LUMEN_ASSERT(idx < MAX_DYNAMIC_RESOURCES, "Resource does not belong to this dynamic pool");
		availability_mask[idx / DYNAMIC_POOL_WORD_BITS] &= ~(u64(1) << (idx % DYNAMIC_POOL_WORD_BITS));
	}

	lm::String arena_name;
	lm::Arena* arena = nullptr;
	lm::FixedArray<T> data;
	u64 availability_mask[DYNAMIC_POOL_WORD_COUNT] = {0};
};

namespace drm {
thread_local DynamicPool<vk::Buffer> _buffer_pool(CSTR("Dynamic Buffer Pool Arena"));
thread_local DynamicPool<vk::Texture> _texture_pool(CSTR("Dynamic Texture Pool Arena"));

vk::Buffer* get(const vk::BufferDesc& desc) {
	vk::Buffer* buffer = _buffer_pool.get();
	vk::buffer_create(buffer, desc);
	return buffer;
}
vk::Texture* get(const vk::TextureDesc& desc) {
	vk::Texture* tex = _texture_pool.get();
	vk::texture_create(tex, desc);
	return tex;
}

void destroy(vk::Buffer* buffer) {
	if (buffer == nullptr) return;
	vk::buffer_destroy(buffer);
	_buffer_pool.remove(buffer);
}
void destroy(vk::Texture* tex) {
	if (tex == nullptr) return;
	vk::texture_destroy(tex);
	_texture_pool.remove(tex);
}

}  // namespace drm
