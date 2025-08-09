#include "Framework/Window.h"
#include "Framework/HashMap.h"
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

	lm::Array<int> arr = lm::array_create<int>(arena);
	arr.push_back(5);
	arr.push_back(10);
	arr.push_back(15);
	arr.push_back(20);
	for(int val : arr) {
		LUMEN_INFO("Array value: {}", val);
	}

	auto hm = lm::hash_map_create<int, int>(arena);

	hm.insert(1, 100);
	hm.insert(2, 200);
	hm.insert(682, 800);

	auto entry = hm.find(682);
	assert(entry != nullptr);
	auto entry2 = hm.find(3);
	assert(entry2 == nullptr);
	// LUMEN_INFO("Found entry: key = {}, value = {}", entry->key, entry->value);

	for(auto& e : hm) {
		LUMEN_INFO("HashMap entry: key = {}, value = {}", e.key, e.value);
	}

	__debugbreak();
}
#endif
