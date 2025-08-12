#include "String.h"
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

namespace lm {
String str_from_f64(Arena* arena, double val, bool cstr) {
	char buf[32];
	i32 num_chars = stbsp_snprintf(buf, sizeof(buf), "%.9g", val);
	String result = {0};
	result.size = num_chars + cstr;
	char* data = (char*)arena->allocate(result.size);
	memmove(data, buf, sizeof(buf));
	result.data = data;
	if (cstr) result.data[num_chars] = 0;
	return result;
}

String str_to_cstr(Arena* arena, const String& str) {
	char* data = (char*)arena->allocate(str.size + 1);
	memmove(data, str.data, str.size);
	data[str.size] = 0;
	String result = {0};
	result.data = data;
	result.size = str.size + 1;
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

String str_chop(const String& str, u64 start, u64 end) {
	if (start >= str.size) return {nullptr, 0};
	if (end == -1 || end > str.size) end = str.size;
	if (start >= end) return {nullptr, 0};
	String result = {0};
	result.data = str.data + start;
	result.size = end - start;
	return result;
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

static String str_from_number(Arena* arena, u64 abs_val, bool negative, bool cstr) {
	u32 num_chars = 0;
	for (u64 v = abs_val; v != 0; ++num_chars) {
		v /= 10;
	}
	String result = {0};
	result.size = num_chars + cstr + negative;
	result.data = (char*)arena->allocate(result.size);
	if (cstr) result.data[result.size - 1] = 0;
	if (negative) result.data[0] = '-';
	for (u32 i = 0; i < num_chars; i++) {
		result.data[num_chars - 1 - i + negative] = '0' + abs_val % 10;
		abs_val /= 10;
	}
	return result;
}

String str_from_u64(Arena* arena, u64 val, bool cstr) { return str_from_number(arena, val, false, cstr); }
String str_from_s64(Arena* arena, s64 val, bool cstr) {
	bool negative = val < 0;
	u64 abs_val = negative ? (u64)(-(val + 1)) + 1 : (u64)val;
	return str_from_number(arena, abs_val, negative, cstr);
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

}  // namespace lm
