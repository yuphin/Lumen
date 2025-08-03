#pragma once

namespace os {
size_t get_page_size();
void* reserve(size_t reserve_size);
bool commit(void* ptr, size_t commit_size);
}  // namespace os