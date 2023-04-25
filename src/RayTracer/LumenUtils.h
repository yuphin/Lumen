#pragma once
#include "LumenScene.h"
#include "Framework/VkUtils.h"

BlasInput to_vk_geometry(LumenPrimMesh& prim, VkDeviceAddress vertex_address, VkDeviceAddress index_address);
