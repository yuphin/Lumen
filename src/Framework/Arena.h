#include "LumenPCH.h"


struct Arena;
struct TempArena {
    Arena* arena;
    size_t saved_base;
    void pop();
};

struct Arena {
    Arena* next;
    uint8_t *data;
    size_t global_offset;
    size_t local_offset;
    size_t end_reserved;
    size_t end_committed;
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    void clear();
    TempArena temp();
};
Arena* arena_create(size_t reserve_size = MB(64), size_t commit_size = KB(64), size_t header_alignment = -1); 
