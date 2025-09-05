#pragma once
#include "Memory.h"
#include "Hash.h"
#include "String.h"
namespace lm {

inline constexpr u64 HASH_MAP_LINEAR_MIN_CAPACITY = 32;
inline constexpr u64 HASH_MAP_HASH_EMPTY = 0;
inline constexpr u64 HASH_MAP_HASH_EMPTY_BUT_RESIZING = 1;
inline constexpr u64 HASH_MAP_HASH_DELETED = 2;
inline constexpr u64 HASH_MAP_LOAD_PERCENTAGE_THRESHOLD = 70;

template <typename T1, typename T2>
struct HashMapEntry {
	u64 hash;
	T1 key;
	T2 value;
};

// For hash set
struct Empty {};
template <typename T>
struct HashMapEntry<T, Empty> {
	u64 hash;
	T key;
};

template <typename T1, typename T2, u64 (*hash_func)(const T1&)>
struct HashMapProbed {
	HashMapEntry<T1, T2>* data = nullptr;
	u64 size = 0;
	// Also includes the deleted entries
	u64 num_slots = 0;
	u64 capacity = 0;
	Arena* arena_node = nullptr;

	void resize(u64 new_capacity) {
		new_capacity = util::next_pow2(new_capacity);
		using HashMapEntryType = HashMapEntry<T1, T2>;
#if 0
		Arena* new_arena_node;
		// When we re-size, we need to allocate into a new block
		HashMapEntryType* new_data = (HashMapEntryType*)arena_node->allocate(
			new_capacity * sizeof(HashMapEntryType), alignof(HashMapEntryType), &new_arena_node,
			/*zero_initialize=*/true,
			/*exclusive_block_reserve_size=*/new_capacity * sizeof(HashMapEntryType));


		HashMapEntryType* old_data = data;
		data = new_data;
		size = 0;
		num_slots = 0;
		arena_node->local_offset -= capacity * sizeof(HashMapEntryType);
		arena_node = new_arena_node;
		u64 old_capacity = capacity;
		capacity = new_capacity;

		for (u64 i = 0; i < old_capacity; i++) {
			const auto& old_entry = old_data[i];
			if (old_entry.hash > HASH_MAP_HASH_DELETED) {
				if constexpr (!util::is_same<T2, Empty>::value) {
					insert(old_entry.key, old_entry.value);
				} else {
					insert(old_entry.key);
				}
			}
		}
#else
		// Try to resize in-place
		u64 hm_size = capacity * sizeof(HashMapEntryType);
		u64 local_offset_alligned_prev = util::align_pow2(arena_node->local_offset - hm_size, alignof(HashMapEntryType));
		bool is_sequential = (arena_node->data + local_offset_alligned_prev) == (u8*)data ;
		if (is_sequential) {
			arena_node->local_offset -= capacity * sizeof(HashMapEntryType);
		} else {
			LUMEN_WARN("Hash Map: Not resizing in-place. You may want to re-think your hashmap allocation strategy");
		}

		// TODO: Do we need to make sure it's exclusive if initial size isn't given?
		Arena* new_arena_node;
		u64 alloc_size = new_capacity * sizeof(HashMapEntryType);
		HashMapEntryType* new_data =
			(HashMapEntryType*)arena_node->allocate(alloc_size, alignof(HashMapEntryType), &new_arena_node,
													/*zero_initialize=*/false);
		bool is_different_block = new_arena_node != arena_node;

		HashMapEntryType* old_data = data;
		data = new_data;
		u64 old_size = size;
		size = 0;
		num_slots = 0;
		arena_node = new_arena_node;
		u64 old_capacity = capacity;
		capacity = new_capacity;
		for (u64 i = 0; i < old_capacity; i++) {
			if (old_data[i].hash > HASH_MAP_HASH_DELETED) {
				old_data[i].hash = HASH_MAP_HASH_EMPTY_BUT_RESIZING;
			}
		}
		u64 processed = 0;
		u64 scan_idx = 0;
		HashMapEntry<T1, T2> old_entry;
		bool old_entry_found = false;
		while (processed < old_size) {
			if (!old_entry_found) {
				for (; scan_idx < old_capacity; ++scan_idx) {
					if (old_data[scan_idx].hash == HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
						break;
					}
				}
				old_entry = old_data[scan_idx++];
			}
			old_entry_found = insert_during_resize(&old_entry);
			++processed;
		}
		if (!is_different_block && is_sequential) {
			for (u64 i = 0; i < old_capacity; i++) {
				if (old_data[i].hash == HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
					old_data[i].hash = HASH_MAP_HASH_EMPTY;
				}
			}
		}

#endif
	}

	bool insert_during_resize(HashMapEntry<T1, T2>* old_entry) {
		u64 hash = hash_func(old_entry->key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}
		bool old_entry_found = false;
		u64 index = hash & (capacity - 1);

		u64 probe_inc = 1;
		while (data[index].hash > HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
			HashMapEntry<T1, T2>& entry = data[index];
			if (entry.hash == HASH_MAP_HASH_DELETED) {
				--num_slots;
				break;
			} else if (entry.key == old_entry->key) {
				if constexpr (!util::is_same<T2, Empty>::value) {
					entry.value = old_entry->value;
				}
				return old_entry_found;
			}
			index = (index + probe_inc) & (capacity - 1);
			probe_inc++;
		}
		++num_slots;
		++size;

		HashMapEntry<T1, T2> old_entry_copy = *old_entry;
		if (data[index].key != old_entry_copy.key && data[index].hash == HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
			*old_entry = data[index];
			old_entry_found = true;
		}
		data[index].hash = hash;
		data[index].key = old_entry_copy.key;
		if constexpr (!util::is_same<T2, Empty>::value) {
			data[index].value = old_entry_copy.value;
		}
		return old_entry_found;
	}

	HashMapEntry<T1, T2>* insert(const T1& key, const T2& value) {
		// Assumes the capacity is always a power of two
		if (num_slots * 100 > capacity * HASH_MAP_LOAD_PERCENTAGE_THRESHOLD) {
			resize(capacity << 1);
		}
		u64 hash = hash_func(key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}

		u64 index = hash & (capacity - 1);

		u64 probe_inc = 1;
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
		u64 hash = hash_func(key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}
		u64 index = hash & (capacity - 1);
		u64 probe_inc = 1;

		while (data[index].hash > HASH_MAP_HASH_DELETED) {
			if (data[index].hash == hash && data[index].key == key) {
				return &data[index];
			}
			index = (index + probe_inc) & (capacity - 1);
			probe_inc++;
		}
		return nullptr;
	}

	// If the entry doesn't exist, creates it, but doesn't initialize the value
	HashMapEntry<T1, T2>* get_or_create(const T1& key) {
		// Assumes the capacity is always a power of two
		if (num_slots * 100 > capacity * HASH_MAP_LOAD_PERCENTAGE_THRESHOLD) {
			resize(capacity << 1);
		}
		u64 hash = hash_func(key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}

		u64 index = hash & (capacity - 1);

		u64 probe_inc = 1;
		while (data[index].hash != HASH_MAP_HASH_EMPTY) {
			HashMapEntry<T1, T2>& entry = data[index];
			if (entry.hash == HASH_MAP_HASH_DELETED) {
				--num_slots;
				break;
			} else if (entry.key == key) {
				return &data[index];
			}
			index = (index + probe_inc) & (capacity - 1);
			probe_inc++;
		}
		++num_slots;
		++size;
		data[index].hash = hash;
		data[index].key = key;
		data[index].value = {};
		return &data[index];
	}

	HashMapEntry<T1, T2>* remove(const T1& key) {
		u64 hash = hash_func(key);
		if (hash <= HASH_MAP_HASH_DELETED) {
			hash += HASH_MAP_HASH_DELETED + 1;
		}
		u64 index = hash & (capacity - 1);
		u64 probe_inc = 1;

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
		HashMapProbed<T1, T2, hash_func>* map;
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

// Like arrays, hash maps are also always allocated in a new block
template <typename T1, typename T2, u64 (*hash_func)(const T1&) = default_hash<T1>>
HashMapProbed<T1, T2, hash_func> hash_map_create(Arena* arena, u64 initial_capacity = HASH_MAP_LINEAR_MIN_CAPACITY) {
	using HashMapEntryType = HashMapEntry<T1, T2>;
	HashMapProbed<T1, T2, hash_func> map;
	// We also multiply the capacity by 1.5 in case we want to accomodate w.r.t hash map load percentage threshold
	initial_capacity = util::next_pow2(3 * (initial_capacity + 1) >> 1);
	map.capacity = initial_capacity;
	Arena* arena_node;
	map.data = (HashMapEntryType*)arena->allocate(initial_capacity * sizeof(HashMapEntryType),
												  alignof(HashMapEntryType), &arena_node,
												  /*zero_initialize=*/true);
	map.arena_node = arena_node;
	return map;
}

template <typename T1, typename T2, u64 (*hash_func)(const T1&) = default_hash<T1>>
using HashMap = HashMapProbed<T1, T2, hash_func>;

template <typename T, u64 (*hash_func)(const T&) = default_hash<T>>
using HashSet = HashMapProbed<T, Empty, hash_func>;

template <typename T, u64 (*hash_func)(const T&) = default_hash<T>>
HashSet<T, hash_func> hash_set_create(Arena* arena, u64 initial_capacity = HASH_MAP_LINEAR_MIN_CAPACITY) {
	return hash_map_create<T, Empty, hash_func>(arena, initial_capacity);
}
}  // namespace lm