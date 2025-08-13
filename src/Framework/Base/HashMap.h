#pragma once
#include "Memory.h"
#include "Hash.h"
namespace lm {

inline constexpr u64 HASH_MAP_LINEAR_MIN_CAPACITY = 32;
inline constexpr u64 HASH_MAP_HASH_EMPTY = 0;
inline constexpr u64 HASH_MAP_HASH_DELETED = 1;
inline constexpr u64 HASH_MAP_LOAD_PERCENTAGE_THRESHOLD = 70;

template <typename T>
static inline uint64_t get_default_hash(const T& x) {
	if constexpr (std::is_integral_v<T> || std::is_enum_v<T> || std::is_pointer_v<T> || std::is_floating_point_v<T>) {
		if constexpr (std::is_floating_point_v<T>) {
			return sdbm_hash((void*)&x, sizeof(T), HASH_INIT);
		} else {
			return knuth_hash(static_cast<uint64_t>(x) ^ HASH_INIT);
		}
	} else {
		static_assert(false, "get_default_hash: Unsupported type for hashing");
		return 0;
	}
}

template <typename T>
static inline uint64_t get_default_hash(const Array<T>& array) {
	return sdbm_hash((void*)array.data, array.size * sizeof(T), HASH_INIT);
}

template <typename T1, typename T2>
struct HashMapEntry {
	uint64_t hash;
	T1 key;
	T2 value;
};

// For hash set
struct Empty {};
template <typename T>
struct HashMapEntry<T, Empty> {
	uint64_t hash;
	T key;
};


template <typename T1, typename T2, uint64_t (*hash_func)(const T1&)>
struct HashMapLinear {
	HashMapEntry<T1, T2>* data = nullptr;
	u64 size = 0;
	// Also includes the deleted entries
	u64 num_slots = 0;
	u64 capacity = 0;
	Arena* arena_node = nullptr;

	void resize(u64 new_capacity) {
		new_capacity = util::next_pow2(new_capacity);
		arena_ensure_allocated<HashMapEntry<T1, T2>>(arena_node, new_capacity, capacity, /*zero_initialize=*/true);
		capacity = new_capacity;
	}

	HashMapEntry<T1, T2>* insert(const T1& key, const T2& value) {
		// Assumes the capacity is always a power of two
		if (num_slots * 100 > capacity * HASH_MAP_LOAD_PERCENTAGE_THRESHOLD) {
			resize(capacity << 1);
		}
		uint64_t hash = hash_func(key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}

		u64 index = hash & (capacity - 1);

		u32 probe_inc = 1;
		while (data[index].hash != HASH_MAP_HASH_EMPTY) {
			HashMapEntry<T1, T2>& entry = data[index];
			if (entry.hash == HASH_MAP_HASH_DELETED) {
				--num_slots;
				break;
			} else if (entry.key == key) {
				if constexpr (!util::is_same<T2, Empty>::value) {
					entry.value = value;
				}
				return &data[index];
			}
			index = (index + probe_inc) & (capacity - 1);
			probe_inc++;
		}
		++num_slots;
		++size;
		data[index].hash = hash;
		data[index].key = key;
		if constexpr (!util::is_same<T2, Empty>::value) {
			data[index].value = value;
		}
		return &data[index];
	}
	HashMapEntry<T1, T2>* insert(const T1& key) { return insert(key, T2{}); }

	HashMapEntry<T1, T2>* find(const T1& key) {
		uint64_t hash = hash_func(key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}
		u64 index = hash & (capacity - 1);
		u32 probe_inc = 1;

		while (data[index].hash > HASH_MAP_HASH_DELETED) {
			if (data[index].hash == hash && data[index].key == key) {
				return &data[index];
			}
			index = (index + probe_inc) & (capacity - 1);
			probe_inc++;
		}
		return nullptr;
	}

	HashMapEntry<T1, T2>* remove(const T1& key) {
		uint64_t hash = hash_func(key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}
		u64 index = hash & (capacity - 1);
		u32 probe_inc = 1;

		while (data[index].hash > HASH_MAP_HASH_DELETED) {
			if (data[index].hash == hash && data[index].key == key) {
				data[index].hash = HASH_MAP_HASH_DELETED;
				--size;
				return &data[index];
			}
			index = (index + probe_inc) & (capacity - 1);
			probe_inc++;
		}
		return nullptr;
	}

	struct Iterator {
		HashMapLinear<T1, T2, hash_func>* map;
		HashMapEntry<T1, T2>* entry;
		u64 index;

		bool operator==(const Iterator& other) {
			return map == other.map && entry == other.entry && index == other.index;
		}
		bool operator!=(const Iterator& other) { return !(*this == other); }

		HashMapEntry<T1, T2>& operator*() const { return *entry; }
		HashMapEntry<T1, T2>* operator->() const { return entry; }
		Iterator& operator++() {
			do {
				++index;
			} while (index < map->capacity && map->data[index].hash <= HASH_MAP_HASH_DELETED);

			if (index < map->capacity) {
				entry = &map->data[index];
			} else {
				entry = nullptr;
			}
			return *this;
		}
	};

	Iterator begin() {
		Iterator it;
		it.map = this;
		it.index = 0;
		while (it.index < capacity && data[it.index].hash <= HASH_MAP_HASH_DELETED) {
			++it.index;
		}
		if (it.index < capacity) {
			it.entry = &data[it.index];
		} else {
			it.entry = nullptr;
		}
		return it;
	}
	Iterator end() {
		Iterator it;
		it.map = this;
		it.index = capacity;
		it.entry = nullptr;
		return it;
	}
};

template <typename T1, typename T2, uint64_t (*hash_func)(const T1&) = get_default_hash<T1>>
HashMapLinear<T1, T2, hash_func> hash_map_create(Arena* arena) {
	using HashMapEntryType = HashMapEntry<T1, T2>;
	HashMapLinear<T1, T2, hash_func> map;
	map.arena_node = arena;
	map.capacity = HASH_MAP_LINEAR_MIN_CAPACITY;

	Arena* arena_node;
	map.data = (HashMapEntryType*)arena->allocate(HASH_MAP_LINEAR_MIN_CAPACITY * sizeof(HashMapEntryType),
												  alignof(HashMapEntryType), &arena_node);
	memset(map.data, 0, HASH_MAP_LINEAR_MIN_CAPACITY * sizeof(HashMapEntryType));
	map.arena_node = arena_node;
	return map;
}

template <typename T, uint64_t (*hash_func)(const T&) = get_default_hash<T>>
using HashSet = HashMapLinear<T, Empty, hash_func>;

template <typename T, uint64_t (*hash_func)(const T&) = get_default_hash<T>>
HashSet<T, hash_func> hash_set_create(Arena* arena) {
	return hash_map_create<T, Empty, hash_func>(arena);
}
}  // namespace lm