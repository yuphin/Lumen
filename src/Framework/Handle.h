#pragma once
#include "../LumenPCH.h"

namespace lumen {

template <typename T>
class Handle {
   public:
	Handle() : index(0), generation(0) {}
	inline bool valid() const { return generation != 0; }

   private:
	uint16_t index;
	uint16_t generation;
    template <typename V> friend class Pool;

};
}  // namespace lumen
