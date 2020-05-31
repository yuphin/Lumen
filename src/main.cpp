
#include "Lumen.h"

int main() {
	Lumen app(1024, 768, /* fullscreen */ false, /* debug */ true);
	app.run();
	return 0;
}