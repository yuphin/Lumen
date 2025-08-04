
#include "LumenPCH.h"

namespace core {
struct Arena;
struct TempArena {
	Arena* arena;
	size_t saved_base;
	void pop();
};

struct Arena {
	Arena* next;
	uint8_t* data;
	size_t local_offset;
	size_t end_reserved;
	size_t end_committed;
	void* allocate(size_t size, size_t alignment = 8, Arena** arena_node = nullptr);
	void clear();
	TempArena temp();
};

Arena* arena_create(size_t reserve_size = MB(64), size_t commit_size = KB(64), size_t header_alignment = -1);
void arena_ensure_committed(Arena* arena, size_t target_offset);

// A dynamic array that doesn't move its elements when resizing
template <typename T>
struct Array {
	T* data = nullptr;
	size_t size = 0;
	size_t capacity = 0;
	Arena* arena_node = nullptr;

	void ensure_allocated(size_t new_capacity) {
		if (new_capacity <= capacity) return;
		size_t aligned_required_size = util::align_pow2((new_capacity - capacity) * sizeof(T), alignof(T));
		LUMEN_ASSERT(arena_node, "Arena node must be set for Array allocation. Did you forget to use array_create?");
		size_t aligned_offset = util::align_pow2(arena_node->local_offset, alignof(T));
		size_t diff = arena_node->end_reserved - aligned_offset;
		LUMEN_ASSERT(aligned_required_size < diff,
					 "Not enough space in Arena to allocate Array, consider increasing arena block size by {} bytes",
					 aligned_required_size - diff);
		arena_ensure_committed(arena_node, aligned_offset + aligned_required_size);
	}

	void push_back(const T& value) {
		if (size >= capacity) {
			ensure_allocated(capacity == 0 ? 1 : 3 * (capacity >> 1));
		}
		data[size++] = value;
	}

	void clear() { size = 0; }

	void resize(size_t new_size) {
		if (new_size > capacity) {
			ensure_allocated(new_size);
		}
		size = new_size;
	}

	T& operator[](size_t index) {
		assert(index < size);
		return data[index];
	}

	const T& operator[](size_t index) const {
		assert(index < size);
		return data[index];
	}

	T* begin() { return data; }
	T* end() { return data + size; }
	const T* begin() const { return data; }
	const T* end() const { return data + size; }
};

template <typename T>
Array<T> array_create(Arena* arena, size_t initial_capacity = 0) {
	Array<T> arr;
	Arena* arena_node;
	arr.data = (T*)arena->allocate(initial_capacity * sizeof(T), alignof(T), &arena_node);
	arr.size = 0;
	arr.capacity = initial_capacity;
	arr.arena_node = arena_node;
	return arr;
}

}  // namespace core