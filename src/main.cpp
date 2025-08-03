#include "LumenPCH.h"
#include "Framework/Window.h"
#include "Framework/Arena.h"
#include "RayTracer/RayTracer.h"

void window_size_callback(GLFWwindow* window, int width, int height) {}

int main(int argc, char* argv[]) {

	Logger::init();
	Arena* arena = arena_create(MB(64), KB(64), 64);

	void* data_a = arena->allocate(KB(1), KB(1));
	void* data_b = arena->allocate(16, 128);

	void* data_c = arena->allocate(4096, 64);
	void* data_d = arena->allocate(2,8);

	TempArena temp_arena = arena->temp();
	void* temp_data_a = temp_arena.arena->allocate(64, 64);
	temp_arena.pop();

	__debugbreak();

	return 0;
#ifdef _DEBUG
	bool enable_debug = true;
#else
	bool enable_debug = false;
#endif
	for (int i = 0; i < argc; ++i) {
		if (std::strcmp(argv[i], "--validation_enable") == 0 && i + 1 < argc) {
			if (std::strcmp(argv[i + 1], "1") == 0) {
				enable_debug = true;
			} else if (std::strcmp(argv[i + 1], "0") == 0) {
				enable_debug = false;
			}
			++i;
		}
	}
	bool fullscreen = false;
	int width = 1920;
	int height = 1080;
	Logger::init();
	lumen::ThreadPool::init();
	Window::init(width, height, fullscreen);
	{
		RayTracer app(enable_debug, argc, argv);
		app.init();
		while (!Window::should_close()) {
			Window::poll();
			app.update();
		}
		app.cleanup();
	}
	Window::destroy();
	lumen::ThreadPool::destroy();
	return 0;
}