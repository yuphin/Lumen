#pragma once

namespace lm {

struct String;
enum LogLevel { LOG_ERROR = 0, LOG_WARN = 1, LOG_INFO = 2, LOG_TRACE = 3 };

void log(i32 level, const char* str, ...);

}  // namespace lm

#define LUMEN_TRACE(...) lm::log(lm::LOG_TRACE, __VA_ARGS__)
#define LUMEN_WARN(...) lm::log(lm::LOG_WARN, __VA_ARGS__)

#define LUMEN_ERROR(...)                     \
	do {                                     \
		lm::log(lm::LOG_ERROR, __VA_ARGS__); \
		exit(EXIT_FAILURE);                  \
	} while (0)

#ifdef _DEBUG
#define LUMEN_INFO(...) lm::log(lm::LOG_INFO, __VA_ARGS__)
#define LUMEN_ASSERT(x, ...)                     \
	do {                                         \
		if (!(x)) {                              \
			lm::log(lm::LOG_ERROR, __VA_ARGS__); \
			assert(x);                           \
		}                                        \
	} while (0)
#else
#define LUMEN_INFO(...) ((void)0)
#define LUMEN_ASSERT(x, ...) ((void)0)
#endif