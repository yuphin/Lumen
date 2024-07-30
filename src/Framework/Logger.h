#pragma once
#pragma warning(push, 0)
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#pragma warning(pop)
#include <memory>
#include <stdexcept>
namespace Logger {
	void init();
	void set_printer_mode();
	void set_default_mode();
	std::shared_ptr<spdlog::logger>& get();
};

#define LUMEN_TRACE(...) Logger::get()->trace(__VA_ARGS__)
#ifdef _DEBUG
#define LUMEN_INFO(...) Logger::get()->info(__VA_ARGS__)
#else
#define LUMEN_INFO(...)
#define VEX_INFO(...)
#endif
#define LUMEN_WARN(...) Logger::get()->warn(__VA_ARGS__)
#define LUMEN_ERROR(...)                                                \
	{                                                                   \
		Logger::get()->error(__VA_ARGS__);                       \
		throw std::runtime_error("Error: " + std::string(__VA_ARGS__)); \
	}
#define LUMEN_CRITICAL(...) Logger::get()->critical(__VA_ARGS__)
#ifdef _DEBUG
#define LUMEN_ASSERT(x, ...)          \
	{                                 \
		if (!(x)) {                   \
			LUMEN_ERROR(__VA_ARGS__); \
			exit(EXIT_FAILURE);       \
		}                             \
	}
#define LUMEN_ASSERT_PTR(x, ...)      \
	{                                 \
		if (!(x)) {                   \
			LUMEN_ERROR(__VA_ARGS__); \
			return nullptr;           \
		}                             \
	}
#else
#define LUMEN_ASSERT(x, ...)
//#define LUMEN_ASSERT(x)
#define LUMEN_ASSERT_PTR(x, ...)
#endif
#define LUMEN_EXIT(x, ...)  \
	{                       \
		LUMEN_ERROR(x);     \
		exit(EXIT_FAILURE); \
	}
