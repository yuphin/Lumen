#include "lmhpch.h"
#include "core/Logger.h"
#include "Lumen.h"

int main() {
	bool enable_debug = true;
#ifdef NDEBUG
	enable_debug = false;
#endif  
	Logger::init();
	LUMEN_TRACE("Logger initialized");
	Lumen app(1024, 768, /* fullscreen */ false, /* debug */ enable_debug);
	app.run();
	return 0;
}