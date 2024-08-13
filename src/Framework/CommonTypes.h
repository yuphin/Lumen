#pragma once
#include <cstdint>

namespace vk {
struct BufferStatus {
	bool read = false;
	bool write = false;
};
}  // namespace vk
namespace lumen {
struct dim3 {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};

}  // namespace lumen
