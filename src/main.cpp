#include "LumenPCH.h"
#include "Framework/Window.h"
//#include "RTScene.h"
#include "RayTracer/RayTracer.h"
#include <offsetAllocator/offsetAllocator.hpp>

void window_size_callback(GLFWwindow* window, int width, int height) {}

int main(int argc, char* argv[]) {
#ifdef _DEBUG
	bool enable_debug = true;
#else
	bool enable_debug = false;
#endif
	bool fullscreen = false;
	int width = 1920;
	int height = 1080;
	Logger::init();
	lumen::ThreadPool::init();
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

	lumen::ThreadPool::destroy();
	return 0;
}