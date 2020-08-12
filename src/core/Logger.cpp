#include "lmhpch.h"
#include "Logger.h"
std::shared_ptr<spdlog::logger> Logger::s_logger;
void  Logger::init() {
	spdlog::set_pattern("%v%$");
	s_logger = spdlog::stdout_color_mt("Lumen");
	s_logger->set_level(spdlog::level::trace);
}


void Logger::set_printer_mode() {
	s_logger->set_pattern("%v%$");
}


void Logger::set_default_mode() {
	s_logger->set_pattern("%n: %v%$");

}
