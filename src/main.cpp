#include "Framework/Window.h"
#include "Framework/Base/HashMap.h"
#include "RayTracer/RayTracer.h"
#include "Framework/Base/String.h"
#include "Framework/ThreadPool.h"

#if 0
i32 main(i32 argc, char* argv[]) {
#ifdef _DEBUG
	bool enable_debug = true;
#else
	bool enable_debug = false;
#endif
	for (i32 i = 0; i < argc; ++i) {
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
	i32 width = 1920;
	i32 height = 1080;
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
i32 main(i32 argc, char* argv[]) {
	Logger::init();
	lm::Arena* arena = lm::arena_create(GB(1), MB(1));

	lm::Array<i32> arr = lm::array_create<i32>(arena);

	for (i32 i = 0; i < 100'000'000; i++) {
		arr.push_back(i);
	}
	// for (i32 i = 0; i < 100; i++) {
	// 	LUMEN_INFO("Array value: {}", arr[i]);
	// }

	auto hm = lm::hash_map_create<i32, i32>(arena);

	hm.insert(1, 100);
	hm.insert(2, 200);
	hm.insert(682, 800);

	auto entry = hm.find(682);
	assert(entry != nullptr);
	auto entry2 = hm.find(3);
	assert(entry2 == nullptr);
	// LUMEN_INFO("Found entry: key = {}, value = {}", entry->key, entry->value);

	for (auto& e : hm) {
		LUMEN_INFO("HashMap entry: key = {}, value = {}", e.key, e.value);
	}

	lm::String result = lm::str_from_number(arena, 62832387, true);
	lm::String result2 = lm::str_from_f64(arena, 1421.363);

	LUMEN_INFO("Result {}", result.data);
	LUMEN_INFO("Result2 {}", lm::str_to_cstr(arena, result2).data);

	// __debugbreak();
}
#endif
