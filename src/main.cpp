#include "LumenPCH.h"
#include "Framework/Window.h"
#include "Framework/Core.h"
#include "RayTracer/RayTracer.h"

void window_size_callback(GLFWwindow* window, int width, int height) {}

int main(int argc, char* argv[]) {
	Logger::init();
	core::Arena* arena = core::arena_create(MB(8), 64);

	void* data_a = arena->allocate(16, 64);

	void* data_b = arena->allocate(16, 128);

	void* data_c = arena->allocate(4096, 64);
	void* data_d = arena->allocate(2, 8);

	core::TempArena temp_arena = arena->temp();
	core::Array<int> arr = core::array_create<int>(temp_arena.arena, 10);

	for (int i = 0; i < 10; ++i) {
		arr.push_back(i);
	}
	arr.resize(MB(1));
	memset(arr.data, 0, arr.size * sizeof(int));
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