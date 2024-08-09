#pragma once
#include "../LumenPCH.h"
#include "Buffer.h"
#include "Texture.h"

namespace prm {

VkSampler get_sampler(const VkSamplerCreateInfo& sampler_create_info);
vk::Texture* get_texture(const vk::TextureDesc& texture_desc);
vk::Buffer* get_buffer(const vk::BufferDesc& texture_desc);
void replace_texture(vk::Texture* texture, const vk::TextureDesc& texture_desc);
void replace_buffer(vk::Buffer* buffer, const vk::BufferDesc& texture_desc);

}
