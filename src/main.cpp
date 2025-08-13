#include "Framework/Window.h"
#include "Framework/Base/HashMap.h"
#include "RayTracer/RayTracer.h"
#include "Framework/Base/String.h"
#include "Framework/ThreadPool.h"
#include "Framework/Base/OS.h"

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
	hm.insert(2, 400);
	hm.insert(682, 800);
	hm.remove(2);

	auto entry = hm.find(682);
	assert(entry != nullptr);
	auto entry2 = hm.find(3);
	assert(entry2 == nullptr);
	auto entry4 = hm.find(2);
	assert(entry4 == nullptr);
	// LUMEN_INFO("Found entry: key = {}, value = {}", entry->key, entry->value);

	for (auto& e : hm) {
		LUMEN_INFO("HashMap entry: key = {}, value = {}", e.key, e.value);
	}

	auto hs = lm::hash_set_create<int>(arena);
	hs.insert(24);
	hs.insert(24);
	hs.insert(541);
	hs.insert(60);
	for (auto& e : hs) {
		LUMEN_INFO("HashSet entry: key = {}", e.key);
	}

	lm::String result = lm::str_from_u64(arena, 62832387, true);
	lm::String result2 = lm::str_from_s64(arena, -62832387, true);
	lm::String result3 = lm::str_from_f64(arena, 1421.363);

	LUMEN_INFO("CSTR Literal: {}", lm::cstr_literal("Test").data);
	LUMEN_INFO("Result {}", result.data);
	LUMEN_INFO("Result2 {}", result2.data);
	LUMEN_INFO("Result3 {}", result3.data);

	LUMEN_INFO("U64 from str: {}", lm::u64_from_str(lm::str_literal("123456789")));
	LUMEN_INFO("U64 from str: {}", lm::u64_from_str(lm::str_literal("  asd  123456789asd")));
	LUMEN_INFO("U64 from str2: {}", lm::u64_from_str(lm::str_literal("   18446744073709551616")));
	LUMEN_INFO("S64 from str: {}", lm::s64_from_str(lm::str_literal("  -  123456789asd")));

	LUMEN_INFO("{}", f64_from_str(lm::cstr_literal("-1.32e-1")));
	LUMEN_INFO("{}", f64_from_str(lm::cstr_literal("1.2423")));
	LUMEN_INFO("{}", f64_from_str(lm::cstr_literal("-1.2423")));
	LUMEN_INFO("{}", f64_from_str(lm::cstr_literal(".2423")));
	LUMEN_INFO("{}", f64_from_str(lm::cstr_literal("1361763176537161637")));
	LUMEN_INFO("{}", f32_from_str(lm::cstr_literal("1432.34")));
	LUMEN_INFO("{}", f64_from_str(lm::cstr_literal("-0.9814223")));

	os::FileHandle file_handle = os::file_open(lm::cstr_literal("scenes/cornell_box/path.scene"), os::AccessFlag_Read);
	if (file_handle == 0) {
		LUMEN_ERROR("Failed to open file");
		return -1;
	}
	os::FileProperties props = os::file_properties(file_handle);
	LUMEN_INFO("File size: {}, created: {}, modified: {}", props.size, props.created, props.modified);

	lm::String file_content = lm::str_reserve(arena, props.size + 1);

	u64 bytes_read = os::file_read(file_handle, file_content.data);
	file_content.data[props.size] = '\0';

	LUMEN_INFO("Bytes read: {} - File content: {}", bytes_read, file_content.data);

	// __debugbreak();
}
#endif
