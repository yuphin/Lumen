#include "Logger.h"
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

namespace lm {

static void log_bytes_colored(const char* str, u64 n, int level, bool err_stream) {
#if defined(_WIN32) || defined(_WIN64)
	static SRWLOCK _log_lock = SRWLOCK_INIT;
	HANDLE h = GetStdHandle(err_stream ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);

	WORD orig = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
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
			col = FOREGROUND_BLUE;
			break;
	}

	AcquireSRWLockExclusive(&_log_lock);
	{
		SetConsoleTextAttribute(h, col);
		DWORD written;
		WriteFile(h, str, (DWORD)n, &written, nullptr);
		SetConsoleTextAttribute(h, orig);
	}
	ReleaseSRWLockExclusive(&_log_lock);
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
				prefix = "\x1b[34m";
				break;	// original
		}
		suffix = (prefix[0] ? "\x1b[0m" : "");
	}

	if (prefix[0]) ::write(fd, prefix, 5);	// ESC[xxm is 5 bytes
	::write(fd, str, n);
	if (suffix[0]) ::write(fd, suffix, 4);	// ESC[0m is 4 bytes
	// TODO: Mutex on linux
#endif
}

static void log_common(i32 level, const char* fmt, va_list args) {
	thread_local char buffer[8192];
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
