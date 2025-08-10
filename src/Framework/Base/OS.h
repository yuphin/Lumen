#pragma once

namespace os {
u64 get_page_size();
void* reserve(u64 reserve_size);
bool commit(void* ptr, u64 commit_size);
}  // namespace os