#pragma once
#include "Buffer.h"
#include "Texture.h"

namespace vk {
    struct Buffer;
    struct BufferDesc;
    struct TextureDesc;
}
namespace drm {

vk::Buffer* get(const vk::BufferDesc& desc);
vk::Texture* get(const vk::TextureDesc& desc);
void destroy(vk::Buffer* buffer);
void destroy(vk::Texture* tex);

}  // namespace drm
