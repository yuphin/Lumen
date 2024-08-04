#pragma once
#include "../LumenPCH.h"
#include "Buffer.h"

namespace vk {
    struct Buffer;
    struct BufferDesc;

}
namespace lumen {
    class Texture;
}

namespace drm {

vk::Buffer* get(const vk::BufferDesc& desc);
lumen::Texture* get(const vk::TextureDesc& desc);
void destroy_buffer(vk::Buffer* buffer);

}  // namespace drm
