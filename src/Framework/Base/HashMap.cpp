#include "HashMap.h"

namespace lm {

static inline u64& entry_hash_at(u8* entry) { return *(u64*)entry; }

// Returns true if it displaced another pending
// entry into old_entry, which then has to be moved next.
static bool move_entry_during_resize(HashMapBase* map, u8* data, u8* old_entry, u64 entry_size,
									 u64 (*entry_hash)(const void*)) {
	u64 hash = entry_hash(old_entry);
	if (hash <= HASH_MAP_HASH_DELETED) {
		hash += HASH_MAP_HASH_DELETED + 1;
	}
	u64 index = hash & (map->capacity - 1);
	u64 probe_inc = 1;
	while (entry_hash_at(data + index * entry_size) > HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
		index = (index + probe_inc) & (map->capacity - 1);
		probe_inc++;
	}
	++map->num_slots;
	++map->size;

	u8* destination = data + index * entry_size;
	bool old_entry_found = false;
	if (destination != old_entry) {
		if (entry_hash_at(destination) == HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
			u8 scratch[HASH_MAP_MAX_ENTRY_SIZE];
			memcpy(scratch, old_entry, entry_size);
			memcpy(old_entry, destination, entry_size);
			memcpy(destination, scratch, entry_size);
			old_entry_found = true;
		} else {
			assert(entry_hash_at(destination) == HASH_MAP_HASH_EMPTY);
			memcpy(destination, old_entry, entry_size);
			entry_hash_at(old_entry) = HASH_MAP_HASH_EMPTY;
		}
	}
	entry_hash_at(destination) = hash;
	return old_entry_found;
}

void* hash_map_resize(HashMapBase* map, void* data, u64 new_capacity, u64 entry_size, u64 entry_alignment,
					  u64 (*entry_hash)(const void*)) {
	new_capacity = lm::next_pow2(new_capacity);
	u8* old_data = (u8*)data;

	// Try to resize in-place
	Arena* arena_node = map->arena_node;
	u64 hm_size = map->capacity * entry_size;
	u64 local_offset_alligned_prev = lm::align_pow2(arena_node->local_offset - hm_size, entry_alignment);
	bool is_sequential = (arena_node->data + local_offset_alligned_prev) == old_data;
	if (is_sequential) {
		arena_node->local_offset -= hm_size;
	} else {
		LUMEN_WARN("Hash Map: Not resizing in-place. You may want to re-think your hashmap allocation strategy");
	}

	// TODO: Do we need to make sure it's exclusive if initial size isn't given?
	Arena* new_arena_node;
	u64 alloc_size = new_capacity * entry_size;
	u8* new_data = (u8*)arena_node->allocate(alloc_size, entry_alignment, &new_arena_node,
											 /*zero_initialize=*/false);

	const bool in_place = new_data == old_data;
	if (in_place) {
		// Zero out from the tail if in_place
		memset(new_data + hm_size, 0, alloc_size - hm_size);
	} else {
		// Zero out the entire new allocation if not in_place
		memset(new_data, 0, alloc_size);
	}

	u64 old_size = map->size;
	u64 old_capacity = map->capacity;
	map->size = 0;
	map->num_slots = 0;
	map->arena_node = new_arena_node;
	map->capacity = new_capacity;
	for (u64 i = 0; i < old_capacity; i++) {
		u64& hash = entry_hash_at(old_data + i * entry_size);
		if (hash > HASH_MAP_HASH_DELETED) {
			hash = HASH_MAP_HASH_EMPTY_BUT_RESIZING;
		} else if (hash == HASH_MAP_HASH_DELETED) {
			// Reinterpret tombstones as empty
			hash = HASH_MAP_HASH_EMPTY;
		}
	}
	u64 processed = 0;
	u64 scan_idx = 0;
	u8* old_entry = nullptr;
	bool old_entry_found = false;
	while (processed < old_size) {
		if (!old_entry_found) {
			for (; scan_idx < old_capacity; ++scan_idx) {
				if (entry_hash_at(old_data + scan_idx * entry_size) == HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
					break;
				}
			}
			old_entry = old_data + scan_idx++ * entry_size;
		}
		old_entry_found = move_entry_during_resize(map, new_data, old_entry, entry_size, entry_hash);
		++processed;
	}
	if (in_place) {
		for (u64 i = 0; i < old_capacity; i++) {
			u64& hash = entry_hash_at(old_data + i * entry_size);
			if (hash == HASH_MAP_HASH_EMPTY_BUT_RESIZING) {
				hash = HASH_MAP_HASH_EMPTY;
			}
		}
	}
	return new_data;
}

}  // namespace lm
