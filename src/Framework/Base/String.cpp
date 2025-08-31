#include "String.h"
#include "Memory.h"
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

namespace lm {

String str_reserve(Arena* arena, u64 size) {
	String result;
	result.size = size;
	result.data = (char*)arena->allocate(size);
	return result;
}

String str_from_f64(Arena* arena, double val) {
	char buf[32];
	i32 num_chars = stbsp_snprintf(buf, sizeof(buf), "%.9g", val);
	String result;
	result.size = num_chars + 1;
	char* data = (char*)arena->allocate(result.size);
	memmove(data, buf, sizeof(buf));
	result.data = data;
	result.data[num_chars] = '\0';
	return result;
}

String str_from_f32(Arena* arena, float val) { return str_from_f64(arena, val); }

String str_concat(Arena* arena, const String& str1, const String& str2) {
	String result = {};
	result.size = str1.size + str2.size + 1;
	result.data = (char*)arena->allocate(result.size);
	memcpy(result.data, str1.data, str1.size);
	memcpy(result.data + str1.size, str2.data, str2.size);
	result.data[result.size - 1] = '\0';
	return result;
}

bool char_is_digit(char c) { return c >= '0' && c <= '9'; }
bool char_is_upper(char c) { return c >= 'A' && c <= 'Z'; }
bool char_is_lower(char c) { return c >= 'a' && c <= 'z'; }
bool char_is_alpha(char c) { return char_is_upper(c) || char_is_lower(c); }
bool char_is_alnum(char c) { return char_is_digit(c) || char_is_alpha(c); }
bool char_is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
char char_to_upper(char c) {
	if (char_is_lower(c)) {
		return c - ('a' - 'A');
	}
	return c;
}
char char_to_lower(char c) {
	if (char_is_upper(c)) {
		return c + ('a' - 'A');
	}
	return c;
}


String str_from_cstr(Arena* arena, const char* cstr, u64 size) {

	String result = {};
	result.size = size;
	result.data = (char*)arena->allocate(result.size);
	memcpy(result.data, cstr, size);
	return result;
}

String str_substr(const String& str, u64 begin, u64 length) {
	assert(begin < str.size && (begin + length) <= str.size);
	return String((char*)str.data + begin, length);
}

// Radix 10
u64 u64_from_str(const String& str) {
	u64 result = 0;
	for (char c : str) {
		if (char_is_whitespace(c) || !char_is_digit(c)) {
			continue;
		}
		u64 num = c - '0';

		if (result > (U64_MAX / 10) || (result == (U64_MAX / 10) && num > 5)) {
			return U64_MAX;
		}
		result = result * 10 + num;
	}
	return result;
}

s64 s64_from_str(const String& str) {
	assert(str.size > 0);
	bool negative = 0;
	s64 result = 0;

	size_t curr = 0;
	for (size_t curr = 0; curr < str.size; curr++) {
		if (char_is_digit(str[curr])) {
			break;
		}
		if (str[curr] == '-') {
			negative = true;
			curr++;
			break;
		}
	}
	for (; curr < str.size; curr++) {
		char c = str[curr];
		if (char_is_whitespace(c) || !char_is_digit(c)) {
			continue;
		}
		u64 num = c - '0';
		if (result > (I64_MAX / 10) || (result == (I64_MAX / 10) && (num > (7 + negative)))) {
			return negative ? I64_MIN : I64_MAX;
		}
		result = result * 10 + (c - '0');
	}
	return negative ? -result : result;
}

static String str_from_number(Arena* arena, u64 abs_val, bool negative) {
	u32 num_chars = 0;
	for (u64 v = abs_val; v != 0; ++num_chars) {
		v /= 10;
	}
	String result;
	result.size = num_chars + negative + 1;
	result.data = (char*)arena->allocate(result.size);
	result.data[result.size - 1] = '\0';
	if (negative) result.data[0] = '-';
	for (u32 i = 0; i < num_chars; i++) {
		result.data[num_chars - 1 - i + negative] = '0' + abs_val % 10;
		abs_val /= 10;
	}
	return result;
}

String str_from_u64(Arena* arena, u64 val) { return str_from_number(arena, val, false); }
String str_from_u32(Arena* arena, u32 val) { return str_from_number(arena, val, false); }
String str_from_s64(Arena* arena, s64 val) {
	bool negative = val < 0;
	u64 abs_val = negative ? (u64)(-(val + 1)) + 1 : (u64)val;
	return str_from_number(arena, abs_val, negative);
}

f64 f64_from_str(const String& str) {
	f64 result = 0;
	size_t curr = 0;
	bool negative = false;

	while (curr < str.size && char_is_whitespace(str[curr])) ++curr;
	if (curr == str.size) {
		return 0;
	}

	if (str[curr] == '-' || str[curr] == '+') {
		negative = str[curr] == '-';
		curr++;
	}

	// Integer
	while (curr < str.size && char_is_digit(str[curr])) {
		result = result * 10.0 + (str[curr] - '0');
		curr++;
	}

	// Fractional
	if (curr < str.size && str[curr] == '.') {
		curr++;
		f64 frac = 0.0;
		f64 base = 0.1;
		while (curr < str.size && char_is_digit(str[curr])) {
			frac += (str[curr] - '0') * base;
			base *= 0.1;
			++curr;
		}
		result += frac;
	}
	if (curr < str.size && (str[curr] == 'e' || str[curr] == 'E')) {
		++curr;
		bool exp_negative = false;
		if (curr < str.size && (str[curr] == '-' || str[curr] == '+')) {
			exp_negative = (str[curr] == '-');
			++curr;
		}
		u32 exp = 0;
		while (curr < str.size && char_is_digit(str[curr])) {
			exp = exp * 10 + (str[curr] - '0');
			++curr;
		}
		f64 exp_mul = 1.0;
		f64 base = exp_negative ? 0.1 : 10.0;
		for (u32 j = 0; j < exp; ++j) {
			exp_mul *= base;
		}
		result *= exp_mul;
	}
	return negative ? -result : result;
}
f32 f32_from_str(const String& str) { return (f32)f64_from_str(str); }

String str_to_lower(Arena* arena, const String& str) {
	String result = str_reserve(arena, str.size);
	for (u64 i = 0; i < str.size; i++) {
		result.data[i] = char_to_lower(str.data[i]);
	}
	return result;
}

bool str_compare(const String& str1, const String& str2) {
	if (str1.size != str2.size) {
		return false;
	}
	for (u64 idx = 0; idx < str1.size; ++idx) {
		if (str1[idx] != str2[idx]) {
			return false;
		}
	}
	return true;
}


u64 str_rfind(const String& str1, const String& str2) {
	assert(str1.size >= str2.size);
	u64 str2_idx = str2.size - 1;
	u64 str1_idx = str1.size - 1;
	for (; str2_idx >= 0; --str1_idx, --str2_idx) {
		if (str1[str1_idx] != str2[str2_idx]) {
			return U64_MAX;
		}
	}
	return str1_idx;
}

bool str_ends_with(const lm::String& str1, const lm::String& str2) {
	return str_rfind(str1, str2) != U64_MAX;
}

bool String::operator==(const String& other) { return str_compare(*this, other); }

}  // namespace lm
