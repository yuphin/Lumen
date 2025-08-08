#include "Framework/Window.h"
#include "Framework/Base.h"
#include "RayTracer/RayTracer.h"

#if 0
int main(int argc, char* argv[]) {
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
	ThreadPool::init();
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
	ThreadPool::destroy();
	return 0;
}
#else
int main(int argc, char* argv[]) {
	Logger::init();
	lm::Arena* arena = lm::arena_create(MB(8), 64);

	void* data_a = arena->allocate(16, 64);

	void* data_b = arena->allocate(16, 128);

	void* data_c = arena->allocate(4096, 64);
	void* data_d = arena->allocate(2, 8);
}
#endif
