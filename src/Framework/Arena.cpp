#include "../LumenPCH.h"
#include "Arena.h"
#include "OS.h"
#include "Utils.h"

constexpr size_t HEADER_SIZE = sizeof(Arena);
static constexpr size_t ALIGNED_HEADER_SIZE = util::next_pow2(HEADER_SIZE);

static void arena_pop(Arena* arena, size_t target_base) {
	Arena* last_arena = arena;
	for (Arena* next_arena = arena->next; next_arena; last_arena = next_arena, next_arena = next_arena->next) {
		assert(next_arena->global_offset >= target_base);
        next_arena->local_offset = 0;
		next_arena->global_offset = 0;
	}
    assert(last_arena);
	size_t diff = last_arena->global_offset - target_base;
	assert(diff >= 0 && diff <= last_arena->local_offset);
	arena->local_offset -= diff;
	arena->global_offset = target_base;
}

void* Arena::allocate(size_t size, size_t alignment) {
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
		new_arena->global_offset += end_reserved;
		last_block->next = new_arena;
		curr_arena = new_arena;

		local_offset_alligned = 0;
		new_pos = size_aligned;
	}

	LUMEN_ASSERT(new_pos <= curr_arena->end_reserved, "New position must be within the reserved space of the arena");
	if (new_pos > curr_arena->end_committed) {
		size_t commit_size = util::align_pow2(new_pos - curr_arena->end_committed, os::get_page_size());
		bool commited = os::commit(curr_arena->data + curr_arena->end_committed, commit_size);
		LUMEN_ASSERT(commited, "Could not commit memory for Arena");
		curr_arena->end_committed += commit_size;
	}
	void* result = curr_arena->data + local_offset_alligned;
    size_t diff = new_pos - curr_arena->local_offset;
	curr_arena->local_offset = new_pos;
    curr_arena->global_offset += diff;
	return result;
}

void Arena::clear() { arena_pop(this, 0); }

TempArena Arena::temp() {
	TempArena temp;
	Arena* last_arena = nullptr;
	for (Arena* arena = this; arena; last_arena = arena, arena = arena->next);
    assert(last_arena);
	temp.arena = last_arena;
	temp.saved_base = last_arena->global_offset;
	return temp;
}
void TempArena::pop() { 
    arena_pop(arena, saved_base); 
}

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
	os::commit(base, commit_size);
	if (!base) {
		LUMEN_ERROR("Could not allocate memory for Arena");
	}
	Arena* arena = (Arena*)base;

	arena->next = nullptr;
	arena->data = (uint8_t*)base + header_alignment;
	arena->global_offset = 0;
	arena->local_offset = 0;
	arena->end_reserved = reserve_size;
	arena->end_committed = commit_size;
	return arena;
}