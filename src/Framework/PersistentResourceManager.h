#pragma once
#include "../LumenPCH.h"
#include "Buffer.h"
#include "Texture.h"

namespace prm {

VkSampler get_sampler(const VkSamplerCreateInfo& sampler_create_info, bool use_mutex = false);
vk::Texture* get_texture(const vk::TextureDesc& texture_desc, bool use_mutex = false);
vk::Buffer* get_buffer(const vk::BufferDesc& texture_desc, bool use_mutex = false);

void remove(vk::Buffer* buffer);
void remove(vk::Texture* texture);
void destroy();
}
