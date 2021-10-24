#include "LumenPCH.h"
#include "Logger.h"
std::shared_ptr<spdlog::logger> Logger::s_logger;
void Logger::init() {
	spdlog::set_pattern("%v%$");
	s_logger = spdlog::stdout_color_mt("Lumen");
	s_logger->set_level(spdlog::level::trace);
#if defined _MSC_VER
	auto logger_sink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(
		s_logger->sinks().back().get());
	logger_sink->set_color(spdlog::level::trace, logger_sink->GREEN);
#endif
}

void Logger::set_printer_mode() { s_logger->set_pattern("%v%$"); }

void Logger::set_default_mode() { s_logger->set_pattern("%n: %v%$"); }
