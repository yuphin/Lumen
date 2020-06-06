
#include "Lumen.h"

int main() {
	bool enable_debug = true;
#ifdef NDEBUG
	enable_debug = false;
#endif  
	Lumen app(1024, 768, /* fullscreen */ false, /* debug */ enable_debug);
	app.run();
	return 0;
}