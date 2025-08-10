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

}  // namespace lm
