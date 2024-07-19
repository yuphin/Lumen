#pragma once
#include <cstdint>

namespace lumen {
struct dim3 {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};

struct BufferStatus {
	bool read = false;
	bool write = false;
};

}  // namespace lumen
