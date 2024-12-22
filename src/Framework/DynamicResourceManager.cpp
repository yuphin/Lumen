#include "../LumenPCH.h"

#include "Buffer.h"
#include "Texture.h"
#include "DynamicResourceManager.h"

static uint16_t count_leading_ones(uint64_t val) {
	if (val == 0) {
		return 0;
	}
#ifdef _MSC_VER
	return uint16_t(64 - __lzcnt64(val));
#else
	return uint16_t(64 - __builtin_clzll(val));
#endif
}

template <typename T>
class DynamicPool {
   public:
	DynamicPool() {}
	T* get() {
		uint16_t idx = 0;
		uint16_t i = 0;
		for (i = 0; i < 8; i++) {
			auto available_idx = count_leading_ones(availability_mask[i]);
			idx += available_idx;
			if (available_idx < 8) {
				break;
			}
		}
		assert(idx < 512);
		availability_mask[i] |= uint64_t(1) << (idx % 8);
		return &data[idx];
	}
	void remove(T* object) {
		auto idx = (object - data);
		availability_mask[idx >> 6] &= ~(1 << (idx % 8));
	}

   private:
	T data[512];
	uint64_t availability_mask[8] = {0};
};

namespace drm {
thread_local DynamicPool<vk::Buffer> _buffer_pool;
thread_local DynamicPool<vk::Texture> _texture_pool;

vk::Buffer* get(const vk::BufferDesc& desc) {
	auto buffer = _buffer_pool.get();
	vk::create_buffer(buffer, desc);
	return buffer;
}
vk::Texture* get(const vk::TextureDesc& desc) {
	auto tex = _texture_pool.get();
	vk::create_texture(tex, desc);
	return tex;
}

void destroy(vk::Buffer* buffer) {
	if (buffer == nullptr) return;
	vk::destroy_buffer(buffer);
	_buffer_pool.remove(buffer);
}
void destroy(vk::Texture* tex) {
	if (tex == nullptr) return;
	vk::destroy_texture(tex);
	_texture_pool.remove(tex);
}

}  // namespace drm