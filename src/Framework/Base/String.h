#include "Memory.h"
#pragma once
namespace lm {
struct Arena;

struct String {
	char* data = nullptr;
	u64 size = 0;

	String() = default;
	String(char* str, u64 size) : data(str), size(size) {}
	template <size_t N>
    constexpr String(const char (&str)[N]) : data((char*)str), size(N) {}

	
	char& operator[](u64 idx) {
		assert(idx < size);
		return data[idx];
	}
	char operator[](u64 idx) const {
		assert(idx < size);
		return data[idx];
	}

	inline char* begin() { return &data[0]; }
	inline char* end() { return &data[size]; }
	inline const char* begin() const { return &data[0]; }
	inline const char* end() const { return &data[size]; }
};

template <size_t N>
constexpr String literal(const char (&str)[N]) {
	return String{(char*)str, N - 1};
}

template <size_t N>
constexpr String cliteral(const char (&str)[N]) {
	return String{(char*)str, N};
}

bool char_is_digit(char c);
bool char_is_upper(char c);
bool char_is_lower(char c);
bool char_is_alpha(char c);
bool char_is_alnum(char c);
bool char_is_whitespace(char c);
char char_to_upper(char c);
char char_to_lower(char c);

String str_from_f64(Arena* arena, double val, bool cstr = false);
String str_from_u64(Arena* arena, u64 val, bool cstr = false);
String str_from_s64(Arena* arena, s64 val, bool cstr = false);
String str_to_lower(Arena* arena, const String& str);

String str_to_cstr(Arena* arena, const String& str);
String str_chop(const String& str, u64 start = 0, u64 end = -1);

u64 u64_from_str(const String& str);
s64 s64_from_str(const String& str);
f64 f64_from_str(const String& str);
f32 f32_from_str(const String& str);

String str_reserve(Arena* arena, u64 size);

}  // namespace lm