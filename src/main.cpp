#include "lmhpch.h"
#include "Lumen.h"
#include "gfx/Window.h"


void window_size_callback(GLFWwindow* window, int width, int height) {

}

int main() {
#ifdef NDEBUG
	bool enable_debug = false;
#else
	bool enable_debug = true;
#endif  
	bool fullscreen = false;
	int width = 1024;
	int height = 768;
	Logger::init();
	LUMEN_TRACE("Logger initialized");
	Window window(width, height, fullscreen);
	Lumen app(width, height, enable_debug);
	auto wnd_handle = window.get_window_ptr();
	app.init(wnd_handle);
	while (!window.should_close()) {
		window.poll();
		app.update();
	}
	return 0;
}