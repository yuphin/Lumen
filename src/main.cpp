#include "LumenPCH.h"
#include "RTScene.h"
#include "Framework/Window.h"


void window_size_callback(GLFWwindow* window, int width, int height) { }

int main() {
#ifdef NDEBUG
	bool enable_debug = false;
#else
	bool enable_debug = true;
#endif  
	bool fullscreen = false;
	int width = 1600;
	int height = 900;
	Logger::init();
	ThreadPool::init();
	LUMEN_TRACE("Logger initialized");
	Window window(width, height, fullscreen);
	{
		RTScene app(width, height, enable_debug);
		app.init(&window);
		while(!window.should_close()) {
			window.poll();
			app.update();
		}
		app.cleanup();
	}

	ThreadPool::destroy();
	return 0;
}