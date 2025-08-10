#include "Memory.h"
#pragma once
namespace lm {
struct Arena;

struct String {
	char* data;
	u64 size;
	char& operator[](u64 idx) {
		assert(idx < size);
		return data[idx];
	}
};

String str_from_f64(Arena* arena, double val, bool cstr = false);
String str_to_cstr(Arena* arena, const String& str);

template <typename NumType>
String str_from_number(Arena* arena, NumType val, bool cstr = false) {
	u32 num_chars = 0;
	for (uint64_t v = val; v != 0; ++num_chars) {
		v /= 10;
	}
	String result = {0};
	result.size = num_chars + cstr;
	result.data = (char*)arena->allocate(result.size);
	for (u32 i = 0; i < num_chars; i++) {
		result.data[num_chars - 1 - i] = '0' + val % 10;
		val /= 10;
	}
	if (cstr) result.data[num_chars] = 0;
	return result;
}

}  // namespace lm