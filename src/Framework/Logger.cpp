#include "../LumenPCH.h"
#include "Logger.h"

namespace Logger {
std::shared_ptr<spdlog::logger> _logger;
void init() {
	spdlog::set_pattern("%v%$");
	_logger = spdlog::stdout_color_mt("Lumen");
	_logger->set_level(spdlog::level::trace);
#if defined _MSC_VER
	auto logger_sink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(_logger->sinks().back().get());
	logger_sink->set_color(spdlog::level::trace, FOREGROUND_GREEN);
	logger_sink->set_color(spdlog::level::info, FOREGROUND_BLUE);
#endif
}

std::shared_ptr<spdlog::logger>& get() { return _logger; }
}  // namespace Logger
