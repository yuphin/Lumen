#include "Memory.h"
#include "OS.h"

namespace lm {
constexpr u64 HEADER_SIZE = sizeof(Arena);
static constexpr u64 ALIGNED_HEADER_SIZE = util::next_pow2(HEADER_SIZE);

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
	u64 commit_size = util::align_pow2(target_offset - arena->end_committed, os::get_page_size());
	LUMEN_INFO("Commiting %llu bytes ( %llu MB) for Arena", commit_size, commit_size / (1024 * 1024));
	bool commited = os::commit(arena->data + arena->end_committed, commit_size);
	memset(arena->data + arena->end_committed, 0, commit_size);
	LUMEN_ASSERT(commited, "Could not commit memory for Arena");
	arena->end_committed += commit_size;
}

ScratchArena::ScratchArena(Arena* arena_) {
	Arena* last_arena = nullptr;
	for (Arena* arena = arena_; arena; last_arena = arena, arena = arena->next);
	assert(last_arena);
	arena = last_arena;
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
		Arena* new_arena = arena_create(glm::max(exclusive_block_reserve_size, size), size, alignment);
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

Arena* arena_create(u64 reserve_size, u64 commit_size, u64 header_alignment) {
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

	return arena;
}

}  // namespace lm
