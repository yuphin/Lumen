#include "Memory.h"
#include "OS.h"
#include "Utils.h"

namespace lm {
constexpr size_t HEADER_SIZE = sizeof(Arena);
static constexpr size_t ALIGNED_HEADER_SIZE = util::next_pow2(HEADER_SIZE);

static void arena_pop(Arena* arena, size_t target_base) {
	Arena* last_arena = arena;
	for (Arena* next_arena = arena->next; next_arena; last_arena = next_arena, next_arena = next_arena->next) {
		next_arena->local_offset = 0;
	}
	assert(last_arena);
	size_t diff = last_arena->local_offset - target_base;
	assert(diff <= last_arena->local_offset);
	arena->local_offset -= diff;
}

void arena_ensure_committed(Arena* arena, size_t target_offset) {
	arena->local_offset = target_offset;
	if (target_offset <= arena->end_committed) return;
	size_t commit_size = util::align_pow2(target_offset - arena->end_committed, os::get_page_size());
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
	saved_base = last_arena->local_offset;
}
ScratchArena::~ScratchArena() { arena_pop(arena, saved_base); }

void* Arena::allocate(size_t size, size_t alignment, Arena** arena_node) {
	Arena* curr_arena = this;
	const size_t size_aligned = util::align_pow2(size, alignment);

	size_t local_offset_alligned = util::align_pow2(curr_arena->local_offset, alignment);
	size_t new_pos = local_offset_alligned + size_aligned;

	Arena* last_block = nullptr;
	for (Arena* arena = curr_arena; arena; last_block = arena, arena = arena->next) {
		local_offset_alligned = util::align_pow2(arena->local_offset, alignment);
		new_pos = local_offset_alligned + size_aligned;

		if (new_pos <= arena->end_reserved) {
			curr_arena = arena;
			last_block = nullptr;
			break;
		}
	}
	if (last_block != nullptr) {
		Arena* new_arena = arena_create(size_aligned, size_aligned, alignment);
		last_block->next = new_arena;
		curr_arena = new_arena;

		local_offset_alligned = 0;
		new_pos = size_aligned;
	}

	LUMEN_ASSERT(new_pos <= curr_arena->end_reserved, "New position must be within the reserved space of the arena");
	arena_ensure_committed(curr_arena, new_pos);
	void* result = curr_arena->data + local_offset_alligned;
	if (arena_node) {
		*arena_node = curr_arena;
	}
	return result;
}

void Arena::clear() { arena_pop(this, 0); }

Arena* arena_create(size_t reserve_size, size_t commit_size, size_t header_alignment) {
	const size_t page_size = os::get_page_size();

	if (header_alignment == -1) {
		header_alignment = ALIGNED_HEADER_SIZE;
	} else {
		header_alignment = header_alignment > ALIGNED_HEADER_SIZE
							   ? util::align_pow2(header_alignment, ALIGNED_HEADER_SIZE)
							   : ALIGNED_HEADER_SIZE;
	}
	reserve_size = util::align_pow2(reserve_size + HEADER_SIZE, page_size);
	commit_size = util::align_pow2(commit_size + HEADER_SIZE, page_size);
	void* base = os::reserve(reserve_size);
	bool commited = os::commit(base, commit_size);
	assert(commited);
	if (!base) {
		LUMEN_ERROR("Could not allocate memory for Arena");
	}
	Arena* arena = (Arena*)base;

	arena->next = nullptr;
	arena->data = (uint8_t*)base + header_alignment;
	arena->local_offset = 0;
	arena->end_reserved = reserve_size;
	arena->end_committed = commit_size;
	return arena;
}
}  // namespace lm
