#include "Logger.h"
#include <stb/stb_sprintf.h>
#include "Framework/Base/String.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>	 // write, isatty, STDOUT_FILENO, STDERR_FILENO
#endif
namespace lm {

static void log_bytes(const char* str, u64 n, bool err_stream) {
#if defined(_WIN32) || defined(_WIN64)
	HANDLE h = GetStdHandle(err_stream ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
	DWORD written;
	WriteFile(h, str, (DWORD)n, &written, nullptr);
#else
	int fd = err_stream ? STDERR_FILENO : STDOUT_FILENO;
	ssize_t written = ::write(fd, str, n);
	(void)written;
#endif
}

static void log_bytes_colored(const char* str, u64 n, int level, bool err_stream) {
#if defined(_WIN32) || defined(_WIN64)
	HANDLE h = GetStdHandle(err_stream ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	WORD orig = 0;
	GetConsoleScreenBufferInfo(h, &csbi);
	orig = csbi.wAttributes;

	WORD col = orig;
	switch (level) {
		case LOG_ERROR:
			col = FOREGROUND_RED | FOREGROUND_INTENSITY;
			break;
		case LOG_WARN:
			col = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		case LOG_INFO:
			col = FOREGROUND_BLUE;
			break;
		case LOG_TRACE:
			col = orig;
			break;
	}

	SetConsoleTextAttribute(h, col);
	DWORD written;
	WriteFile(h, str, (DWORD)n, &written, nullptr);
	SetConsoleTextAttribute(h, orig);

#else
	int fd = err_stream ? STDERR_FILENO : STDOUT_FILENO;
	const bool tty = isatty(fd);

	const char* prefix = "";
	const char* suffix = "";
	if (tty) {
		switch (level) {
			case LOG_ERROR:
				prefix = "\x1b[31m";
				break;	// red
			case LOG_WARN:
				prefix = "\x1b[33m";
				break;	// yellow
			case LOG_INFO:
				prefix = "\x1b[34m";
				break;	// blue
			case LOG_TRACE:
				prefix = "";
				break;	// original
		}
		suffix = (prefix[0] ? "\x1b[0m" : "");
	}

	if (prefix[0]) ::write(fd, prefix, 5);	// ESC[xxm is 5 bytes
	::write(fd, str, n);
	if (suffix[0]) ::write(fd, suffix, 4);	// ESC[0m is 4 bytes
#endif
}

static void log_common(i32 level, const char* fmt, va_list args) {
	thread_local char buffer[4096];
	int bytes = stbsp_vsnprintf(buffer, (int)sizeof(buffer) - 2, fmt, args);
	if (bytes < 0) return;
	buffer[bytes++] = '\n';
	buffer[bytes] = '\0';
	log_bytes_colored(buffer, (u64)bytes, level, level == LOG_ERROR);
}

void log(i32 level, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log_common(level, fmt, args);
	va_end(args);
}
}  // namespace lm
