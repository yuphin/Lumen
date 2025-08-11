#pragma once
#include "Utils.h"
namespace lm {
struct Arena;
struct ScratchArena {
	Arena* arena;
	u64 saved_base;
	ScratchArena() = delete;
	ScratchArena(Arena* arena);
	~ScratchArena();
};

struct Arena {
	Arena* next;
	u8* data;
	u64 local_offset;
	u64 end_reserved;
	u64 end_committed;
	void* allocate(u64 size, u64 alignment = 8, Arena** arena_node = nullptr);
	void clear();
};

Arena* arena_create(u64 reserve_size = MB(64), u64 commit_size = KB(64), u64 header_alignment = -1);
void arena_ensure_committed(Arena* arena, u64 target_offset);

// A dynamic array that doesn't move its elements when resizing
template <typename T>
struct Array {
	T* data = nullptr;
	u64 size = 0;
	u64 capacity = 0;
	Arena* arena_node = nullptr;

	void push_back(const T& value) {
		if (size == capacity) {
			u64 new_capacity = capacity == 0 ? 4 : 3 * (capacity >> 1);
			arena_ensure_allocated<T>(arena_node, new_capacity, capacity);
			capacity = new_capacity;
		}
		data[size++] = value;
	}

	template <typename... Args>
	T& emplace_back(Args&&... args) {
		if (size >= capacity) {
			u64 new_capacity = 0 ? 1 : 3 * (capacity >> 1);
			arena_ensure_allocated<T>(arena_node, new_capacity, capacity);
			capacity = new_capacity;
		}
		T* slot = &data[size++];
		::new ((void*)slot) T(std::forward<Args>(args)...);
		return *slot;
	}

	void clear() { size = 0; }

	void reserve(u64 new_capacity) {
		if(new_capacity <= capacity) return;
		arena_ensure_allocated<T>(arena_node, new_capacity, capacity);
		capacity = new_capacity;
	}
	void resize(u64 new_size) {
		reserve(new_size);
		size = new_size;
	}

	T& operator[](u64 index) {
		assert(index < size);
		return data[index];
	}

	T* begin() { return data; }
	T* end() { return data + size; }
};

template <typename T>
Array<T> array_create(Arena* arena, u64 initial_capacity = 0) {
	Array<T> arr;
	Arena* arena_node;
	arr.data = (T*)arena->allocate(initial_capacity * sizeof(T), alignof(T), &arena_node);
	arr.size = 0;
	arr.capacity = initial_capacity;
	arr.arena_node = arena_node;
	return arr;
}

template <typename T>
void arena_ensure_allocated(Arena* arena, u64 new_capacity, u64 old_capacity, u64 zero_initialize = false) {
	assert(new_capacity > old_capacity);
	u64 bytes_needed = (new_capacity - old_capacity) * sizeof(T);
	LUMEN_ASSERT(arena, "Did you forget to use array_create?");
	u64 aligned_offset = util::align_pow2(arena->local_offset, alignof(T));
	u64 diff = arena->end_reserved - aligned_offset;
	LUMEN_ASSERT(bytes_needed <= diff, "Not enough space in Arena, consider increasing arena block size by {} bytes",
				 bytes_needed - diff);
	arena_ensure_committed(arena, aligned_offset + bytes_needed);
	if (zero_initialize) {
		memset(arena->data + aligned_offset, 0, bytes_needed);
	}
}

}  // namespace lm