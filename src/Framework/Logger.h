#pragma once
#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#pragma warning(pop)
namespace Logger {
void init();
std::shared_ptr<spdlog::logger>& get();
};	// namespace Logger

#define LUMEN_TRACE(...) Logger::get()->trace(__VA_ARGS__)

#define LUMEN_WARN(...) Logger::get()->warn(__VA_ARGS__)
#define LUMEN_ERROR(...)                   \
	{                                      \
		Logger::get()->error(__VA_ARGS__); \
		exit(EXIT_FAILURE);                \
	}

#define LUMEN_ERROR_LOG(...)               \
	{                                      \
		Logger::get()->error(__VA_ARGS__); \
	}
#define LUMEN_CRITICAL(...) Logger::get()->critical(__VA_ARGS__)
#ifdef _DEBUG
#define LUMEN_INFO(...) Logger::get()->info(__VA_ARGS__)
#define LUMEN_ASSERT(x, ...)          \
	{                                 \
		if (!(x)) {                   \
			LUMEN_ERROR_LOG(__VA_ARGS__); \
			assert(x);                \
		}                             \
	}
#else
#define LUMEN_INFO(...)
#define LUMEN_ASSERT(x, ...)
#endif