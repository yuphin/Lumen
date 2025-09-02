#pragma once
namespace lm {
struct Arena;
struct String;

struct String {
	char* data = nullptr;
	u64 size = 0;

	String() = default;
	String(char* str, u64 size) : data(str), size(size) {}
	template <size_t N>
	// Our strings don't end with null terminator by default
	constexpr String(const char (&str)[N]) : data((char*)str), size(N - 1) {}

	bool operator==(const String& other);

	inline char& operator[](u64 idx) {
		assert(idx < size);
		return data[idx];
	}
	inline char operator[](u64 idx) const {
		assert(idx < size);
		return data[idx];
	}

	inline bool operator!=(const String& other) { return !(*this == other); }
	inline char* begin() { return &data[0]; }
	inline char* end() { return &data[size]; }
	inline const char* begin() const { return &data[0]; }
	inline const char* end() const { return &data[size]; }
	inline bool empty() { return size == 0; }
	inline bool is_cstr() const { return data[size - 1] == '\0'; }
};
template <size_t N>
constexpr String cstr(const char (&str)[N]) {
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

String str_from_cstr(Arena* arena, const char* cstr, u64 size);
String str_to_cstr(Arena* arena, const String& str);
String str_from_f64(Arena* arena, double val);
String str_from_f32(Arena* arena, float val);
String str_from_u64(Arena* arena, u64 val);
String str_from_u32(Arena* arena, u32 val);
String str_from_s64(Arena* arena, s64 val);
String str_to_lower(Arena* arena, const String& str);
String str_substr(const String& str, u64 begin, u64 length);
String str_reserve(Arena* arena, u64 size);
String str_concat(Arena* arena, const String& str1, const String& str2);
bool str_compare(const String& str1, const String& str2);
u64 str_rfind(const String& str1, const String& str2);
u64 str_rfind_any(const String& str1, const String& chars);
bool str_ends_with(const lm::String& str1, const lm::String& str2);
u64 u64_from_str(const String& str);
s64 s64_from_str(const String& str);
u64 u32_from_str(const String& str);
s64 i32_from_str(const String& str);
f64 f64_from_str(const String& str);
f32 f32_from_str(const String& str);

}  // namespace lm