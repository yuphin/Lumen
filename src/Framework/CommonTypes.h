#pragma once
#include <cstdint>
struct dim3u {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};

struct BufferStatus {
	bool read = false;
	bool write = false;
};
