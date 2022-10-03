#include "LumenPCH.h"
#include "Framework/Window.h"
//#include "RTScene.h"
#include "RayTracer/RayTracer.h"

void window_size_callback(GLFWwindow* window, int width, int height) {}

int main(int argc, char* argv[]) {
#ifdef _DEBUG
	bool enable_debug = true;
#else
	bool enable_debug = false;
#endif
	bool fullscreen = false;
	int width = 1600;
	int height = 900;
	Logger::init();
	ThreadPool::init();
	Window window(width, height, fullscreen);
	{
		RayTracer app(width, height, enable_debug, argc, argv);
		app.init(&window);
		while (!window.should_close()) {
			window.poll();
			app.update();
		}
		app.cleanup();
	}

	ThreadPool::destroy();
	return 0;
}