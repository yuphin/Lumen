#pragma  once
namespace lm {
struct Arena;
struct ScratchArena {
	Arena* arena;
	size_t saved_base;
	ScratchArena() = delete;
	ScratchArena(Arena* arena);
	~ScratchArena();
};

struct Arena {
	Arena* next;
	uint8_t* data;
	size_t local_offset;
	size_t end_reserved;
	size_t end_committed;
	void* allocate(size_t size, size_t alignment = 8, Arena** arena_node = nullptr);
	void clear();
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

	void push_back(const T& value) {
		if (size == capacity) {
			size_t new_capacity = capacity == 0 ? 4 : 3 * (capacity >> 1);
			arena_ensure_allocated<Array>(arena_node, new_capacity, capacity);
			capacity = new_capacity;
		}
		data[size++] = value;
	}

	template <typename... Args>
	T& emplace_back(Args&&... args) {
		if (size >= capacity) {
			size_t new_capacity = 0 ? 1 : 3 * (capacity >> 1);
			arena_ensure_allocated<Array>(arena_node, new_capacity, capacity);
			capacity = new_capacity;
		}
		T* slot = &data[size++];
		::new ((void*)slot) T(std::forward<Args>(args)...);
		return *slot;
	}

	void clear() { size = 0; }

	void resize(size_t new_size) {
		if (new_size > capacity) {
			arena_ensure_allocated<Array>(arena_node, new_size, capacity);
			capacity = new_size;
		}
		size = new_size;
	}

	T& operator[](size_t index) {
		assert(index < size);
		return data[index];
	}

	T* begin() { return data; }
	T* end() { return data + size; }
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

template <typename T>
void arena_ensure_allocated(Arena* arena, size_t new_capacity, size_t old_capacity, size_t zero_initialize = false) {
	assert(new_capacity > old_capacity);
	size_t aligned_required_size = util::align_pow2((new_capacity - old_capacity) * sizeof(T), alignof(T));
	LUMEN_ASSERT(arena, "Did you forget to use array_create?");
	size_t aligned_offset = util::align_pow2(arena->local_offset, alignof(T));
	size_t diff = arena->end_reserved - aligned_offset;
	LUMEN_ASSERT(aligned_required_size < diff,
				 "Not enough space in Arena, consider increasing arena block size by {} bytes",
				 aligned_required_size - diff);
	arena_ensure_committed(arena, aligned_offset + aligned_required_size);
	if(zero_initialize) {
		memset(arena->data + aligned_offset, 0, aligned_required_size);
	}
}

}  // namespace lm