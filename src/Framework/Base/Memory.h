#pragma once
#include "Utils.h"
#include "String.h"

namespace lm {

enum ArenaFlags : u32 { ARENA_FLAG_NONE = 0, ARENA_FLAG_RESERVED = 1 << 0, ARENA_FLAG_SCRATCH = 1 << 1 };

struct Arena;
struct ScratchArena {
	Arena* arena;
	u64 saved_base;
	ScratchArena() = delete;
	ScratchArena(Arena* arena);
	~ScratchArena();
};

struct Arena {
	lm::String name;
	Arena* next;
	u8* data;
	u64 local_offset;
	u64 end_reserved;
	u64 end_committed;
	u32 flags;
	void* allocate(u64 size, u64 alignment = 8, Arena** arena_node = nullptr, bool zero_initialize = true,
				   u64 exclusive_block_reserve_size = 0);
	void clear();
};

struct ArenaStats {
	lm::String name;
	u64 id = 0;
	u64 used = 0;
	u64 committed = 0;
	u64 reserved = 0;
	u32 block_count = 0;
};

struct ArenaMemorySummary {
	u64 used = 0;
	u64 committed = 0;
	u64 reserved = 0;
	u32 arena_count = 0;
	u32 block_count = 0;
};

using ArenaStatsCallback = void (*)(const ArenaStats& stats);

constexpr u64 MIN_ARENA_RESERVE_SIZE = MB(1);
constexpr u64 MIN_ARENA_COMMIT_SIZE = KB(64);

Arena* arena_create(lm::String name, u64 reserve_size = MIN_ARENA_RESERVE_SIZE, u64 commit_size = MIN_ARENA_COMMIT_SIZE,
					u64 header_alignment = -1);
void arena_destroy(Arena* arena);
void arena_ensure_committed(Arena* arena, u64 target_offset);
void arena_get_stats(lm::Arena* arena, u64& used, u64& allocated);
void get_all_arena_stats(u64& used, u64& allocated);
ArenaMemorySummary get_all_arena_stats(ArenaStatsCallback callback = nullptr);

// A dynamic array that doesn't move its elements when resizing
template <typename T, bool GROWABLE = true>
struct Array {
	T* data = nullptr;
	u64 size = 0;
	u64 capacity = 0;
	Arena* arena_node = nullptr;

	void grow() {
		if constexpr (GROWABLE) {
			u64 new_capacity = capacity < 4 ? 4 : capacity + (capacity >> 1);
			arena_ensure_allocated_in_the_same_block<T>(arena_node, new_capacity, capacity);
			capacity = new_capacity;
		} else {
			LUMEN_ASSERT(false, "Array capacity exceeded for fixed array");
		}
	}

	T& push() {
		if (size == capacity) {
			grow();
		}
		return data[size++];
	}

	void push_back(const T& value) {
		if (size == capacity) {
			grow();
		}
		data[size++] = value;
	}
	template <typename... Args>
	T& emplace_back(Args&&... args) {
		if (size == capacity) {
			grow();
		}
		T* slot = &data[size++];
		::new ((void*)slot) T(std::forward<Args>(args)...);
		return *slot;
	}
	void clear() { size = 0; }
	void reserve(u64 new_capacity) {
		static_assert(GROWABLE, "Cannot reserve space in a fixed array");
		if (new_capacity <= capacity) return;
		arena_ensure_allocated_in_the_same_block<T>(arena_node, new_capacity, capacity);
		capacity = new_capacity;
	}
	void resize(u64 new_size) {
		static_assert(GROWABLE, "Cannot resize a fixed array");
		reserve(new_size);
		size = new_size;
	}

	void resize_with_value(u64 new_size, const T& default_value = T{}) {
		static_assert(GROWABLE, "Cannot resize a fixed array");
		reserve(new_size);
		size = new_size;
		for (u64 i = 0; i < new_size; i++) {
			data[i] = default_value;
		}
	}

	T& operator[](u64 index) {
		assert(index < size);
		return data[index];
	}
	T& operator[](u64 index) const {
		assert(index < size);
		return data[index];
	}
	T* begin() const { return data; }
	T* end() const { return data + size; }
	inline bool initialized() const { return arena_node != nullptr; }
	T& back() {
		assert(size > 0);
		return data[size - 1];
	}
	inline bool empty() const { return size == 0; }
	util::Slice<T> to_slice() const { return util::Slice(&data[0], size); }
	util::Slice<T> to_slice() { return util::Slice(&data[0], size); }
};

template <typename T, bool GROWABLE = true>
Array<T, GROWABLE> array_create(Arena* arena, u64 initial_capacity = 0, u64 reserved_capacity = 1024) {
	Array<T, GROWABLE> arr;
	Arena* arena_node;
	arr.data = (T*)arena->allocate(initial_capacity * sizeof(T), alignof(T), &arena_node, /*zero_initialize=*/true,
								   /*exclusive_block_reserve_size=*/reserved_capacity * sizeof(T));
	arr.size = 0;
	arr.capacity = initial_capacity;
	arr.arena_node = arena_node;
	return arr;
}

template <typename T>
void arena_ensure_allocated_in_the_same_block(Arena* arena, u64 new_capacity, u64 old_capacity,
											  bool zero_initialize = false) {
	assert(new_capacity > old_capacity);
	u64 bytes_needed = (new_capacity - old_capacity) * sizeof(T);
	LUMEN_ASSERT(arena, "Did you forget to use array_create?");
	u64 aligned_offset = lm::align_pow2(arena->local_offset, alignof(T));
	u64 diff = arena->end_reserved - aligned_offset;
	LUMEN_ASSERT(bytes_needed <= diff, "Not enough space in Arena, consider increasing arena block size by %llu bytes",
				 bytes_needed - diff);
	arena_ensure_committed(arena, aligned_offset + bytes_needed);
	if (zero_initialize) {
		memset(arena->data + aligned_offset, 0, bytes_needed);
	}
}

template <typename T>
using FixedArray = Array<T, false>;

template <typename T>
FixedArray<T> fixed_array_create(Arena* arena, u64 capacity) {
	// exclusive_block_reserve_size = 0 means no exclusive block
	return array_create<T, false>(arena, capacity, 0);
}

template <typename T>
FixedArray<T> fixed_array_init(Arena* arena, std::initializer_list<T> list) {
	FixedArray<T> arr = fixed_array_create<T>(arena, list.size());
	for (const T& item : list) {
		arr.push_back(item);
	}
	return arr;
}

}  // namespace lm
