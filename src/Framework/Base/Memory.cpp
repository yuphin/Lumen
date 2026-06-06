#include "Memory.h"
#include "OS.h"
#include "SmallArray.h"

namespace lm {
constexpr u64 HEADER_SIZE = sizeof(Arena);
static constexpr u64 ALIGNED_HEADER_SIZE = util::next_pow2(HEADER_SIZE);
static constexpr u64 MAX_REGISTERED_ARENAS = 128;

static SmallArray<Arena*, MAX_REGISTERED_ARENAS> _registered_arenas;

static void arena_pop(Arena* arena, u64 target_base) {
	for (Arena* next_arena = arena->next; next_arena; next_arena = next_arena->next) {
		next_arena->local_offset = 0;
	}
	assert(target_base <= arena->local_offset);
	u64 diff = arena->local_offset - target_base;
	arena->local_offset -= diff;
}

void arena_ensure_committed(Arena* arena, u64 target_offset) {
	arena->local_offset = target_offset;
	if (target_offset <= arena->end_committed) return;
	u64 commit_size =
		glm::max(MIN_ARENA_COMMIT_SIZE, util::align_pow2(target_offset - arena->end_committed, os::get_page_size()));
	LUMEN_WARN("Committing %llu bytes ( %f MB) for: %s", commit_size, commit_size / (1024.0 * 1024), arena->name.data);
	bool commited = os::commit(arena->data + arena->end_committed, commit_size);
	memset(arena->data + arena->end_committed, 0, commit_size);
	LUMEN_ASSERT(commited, "Could not commit memory for Arena");
	arena->end_committed += commit_size;
}

void arena_get_stats(lm::Arena* arena, u64& used, u64& allocated) {
	for (lm::Arena* curr = arena; curr; curr = curr->next) {
		used += curr->local_offset;
		allocated += curr->end_committed;
	}
}

void get_all_arena_stats(u64& used, u64& allocated) {
	for (u64 i = 0; i < _registered_arenas.size; i++) {
		arena_get_stats(_registered_arenas[i], used, allocated);
	}
}

ScratchArena::ScratchArena(Arena* arena_) {
	Arena* curr = arena_;
	if (curr->local_offset == 0) {
		arena = curr;
	} else {
		// Find the first arena block whose next is empty
		// Example:
		// Arena a (offset 24 / 1024) -> Arena b (offset 55 / 1024) -> Arena c (offset 0)
		// will return b
		for (; curr->next && curr->next->local_offset != 0; curr = curr->next);
		arena = curr;
	}
	arena->flags |= ARENA_FLAG_SCRATCH;
	saved_base = arena->local_offset;
}
ScratchArena::~ScratchArena() {
	arena_pop(arena, saved_base);
	arena->flags &= ~ARENA_FLAG_SCRATCH;
}

void* Arena::allocate(u64 size, u64 alignment, Arena** arena_node, bool zero_initialize,
					  u64 exclusive_block_reserve_size) {
	Arena* curr_arena = this;

	u64 local_offset_alligned = util::align_pow2(curr_arena->local_offset, alignment);
	u64 new_pos = local_offset_alligned + size;

	bool exclusive_block = exclusive_block_reserve_size > 0;
	Arena* last_block = nullptr;
	for (Arena* arena = curr_arena; arena; last_block = arena, arena = arena->next) {
		if ((arena->flags & ARENA_FLAG_RESERVED) || (exclusive_block && local_offset_alligned > 0)) {
			continue;
		}
		if (arena->flags & ARENA_FLAG_SCRATCH && (this->flags & ARENA_FLAG_SCRATCH) == 0) {
			LUMEN_ASSERT(false, "Cannot allocate after a scratch block");
		}
		local_offset_alligned = util::align_pow2(arena->local_offset, alignment);
		new_pos = local_offset_alligned + size;

		if (new_pos <= arena->end_reserved) {
			curr_arena = arena;
			last_block = nullptr;
			break;
		}
	}
	if (last_block != nullptr) {
		u64 reserve_size = glm::max(MIN_ARENA_RESERVE_SIZE, glm::max(exclusive_block_reserve_size, size));
		u64 commit_size = glm::max(MIN_ARENA_COMMIT_SIZE, size);
		Arena* new_arena = arena_create(curr_arena->name, reserve_size, commit_size, alignment);
		last_block->next = new_arena;
		curr_arena = new_arena;

		local_offset_alligned = 0;
		new_pos = size;
	}

	LUMEN_ASSERT(new_pos <= curr_arena->end_reserved, "New position must be within the reserved space of the arena");
	arena_ensure_committed(curr_arena, new_pos);
	curr_arena->flags |= exclusive_block ? ARENA_FLAG_RESERVED : ARENA_FLAG_NONE;
	void* result = curr_arena->data + local_offset_alligned;
	if (arena_node) {
		*arena_node = curr_arena;
	}
	if (zero_initialize) {
		memset(result, 0, size);
	}
	return result;
}

void Arena::clear() { arena_pop(this, 0); }

Arena* arena_create(lm::String name, u64 reserve_size, u64 commit_size, u64 header_alignment) {
	LUMEN_ASSERT(name.is_cstr(), "Arena name must be a C string");
	const u64 page_size = os::get_page_size();

	if (header_alignment == -1) {
		header_alignment = ALIGNED_HEADER_SIZE;
	} else {
		header_alignment = header_alignment > ALIGNED_HEADER_SIZE
							   ? util::align_pow2(header_alignment, ALIGNED_HEADER_SIZE)
							   : ALIGNED_HEADER_SIZE;
	}
	reserve_size = util::align_pow2(reserve_size + HEADER_SIZE, page_size);
	commit_size = glm::min(reserve_size, util::align_pow2(commit_size + HEADER_SIZE, page_size));
	void* base = os::reserve(reserve_size);
	bool commited = os::commit(base, commit_size);
	assert(commited);
	if (!base) {
		LUMEN_ERROR("Could not allocate memory for Arena");
	}
	Arena* arena = (Arena*)base;

	arena->next = nullptr;
	arena->data = (u8*)base + header_alignment;
	arena->local_offset = 0;
	// @Performance: Perhaps keeping the ends to the requested reserve_size/commit_size
	// would be more beneficial. That way we can have multiple blocks for a single page
	// This might be useful in the context of Arrays and other data structures
	arena->end_reserved = reserve_size - header_alignment;
	arena->end_committed = commit_size - header_alignment;
	arena->flags = ARENA_FLAG_NONE;
	arena->name = name;

	_registered_arenas.push_back(arena);
	return arena;
}

void arena_destroy(lm::Arena* arena) {
	// TODO: Should we decommit?
	arena->clear();
	u64 found_idx = -1;
	for(u64 i = 0; i < _registered_arenas.size; i++) {
		if(_registered_arenas[i] == arena) {
			found_idx = i;
			break;
		}
	}
	assert(found_idx != -1);
	_registered_arenas.erase_unordered(found_idx);
}

}  // namespace lm
