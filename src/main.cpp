#include "Framework/Window.h"
#include "Framework/Base/HashMap.h"
#include "RayTracer/RayTracer.h"
#include "Framework/Base/String.h"
#include "Framework/ThreadPool.h"
#include "Framework/Base/OS.h"

// #undef USE_VALIDATION_LAYERS

#if 1
i32 main(i32 argc, char* argv[]) {
#ifdef USE_VALIDATION_LAYERS
	bool enable_debug = true;
#else
	bool enable_debug = false;
#endif
	bool fullscreen = false;
	bool on_second_monitor = false;
	i32 width = 1920;
	i32 height = 1080;
	for (i32 i = 0; i < argc; ++i) {
		if (std::strcmp(argv[i], "--validation_enable") == 0 && i + 1 < argc) {
			if (std::strcmp(argv[i + 1], "1") == 0) {
				enable_debug = true;
			} else if (std::strcmp(argv[i + 1], "0") == 0) {
				enable_debug = false;
			}
			++i;
		} else if (std::strcmp(argv[i], "--on-second-monitor") == 0) {
			on_second_monitor = true;
		} else if (std::strcmp(argv[i], "--fullscreen") == 0) {
			fullscreen = true;
		}
	}
	tp::init();
	Window::init(width, height, fullscreen, on_second_monitor);
	{
		ray_tracer::init(enable_debug, argc, argv);
		while (!Window::should_close()) {
			Window::poll();
			ray_tracer::update();
		}
		ray_tracer::cleanup();
	}
	Window::destroy();
	tp::destroy();
	return 0;
}
#else

lm::String random_string(lm::Arena* arena, u64 length) {
	lm::ScratchArena scratch = arena;
	lm::String result = lm::str_reserve(scratch.arena, length + 1);
	for (u64 i = 0; i < length; i++) {
		result.data[i] = 'a' + (rand() % 26);
	}
	result.data[length] = '\0';
	return result;
}

std::string random_std_string(u64 length) {
	std::string result;
	result.resize(length);
	for (u64 i = 0; i < length; i++) {
		result[i] = 'a' + (rand() % 26);
	}
	return result;
}

void hash_set_u64_test() {
	LUMEN_TRACE("----Hash Set Test Begin----");
	lm::Arena* arena = lm::arena_create(GB(1));
	auto hs = lm::hash_set_create<u64>(arena, 32);
	std::unordered_set<u64> stdhs;
	auto time_begin = std::chrono::high_resolution_clock::now();
	constexpr u64 NUM_INSERTS = 1024 * 32;
	for (u64 i = 0; i < NUM_INSERTS; i++) {
		if (i % 1000000 == 0) {
			printf("Inserted %llu items into hash set\n", i);
		}
		hs.insert(rand());
	}
	auto time_end = std::chrono::high_resolution_clock::now();
	LUMEN_INFO("Time taken for lm::hash_set: %f seconds\n",
			   std::chrono::duration<double>(time_end - time_begin).count());

	time_begin = std::chrono::high_resolution_clock::now();
	for (u64 i = 0; i < NUM_INSERTS; i++) {
		if (i % 1000000 == 0) {
			printf("Inserted %llu items into hash set\n", i);
		}
		hs.insert(rand());
	}
	time_end = std::chrono::high_resolution_clock::now();

	LUMEN_INFO("Time taken for std::unordered_set: %f seconds\n",
			   std::chrono::duration<double>(time_end - time_begin).count());
	LUMEN_TRACE("----Hash Set Test END----");
}

void hash_set_str_test() {
	LUMEN_TRACE("----Hash Set Test Begin----");
	lm::Arena* arena = lm::arena_create(GB(1));
	auto hs = lm::hash_set_create<lm::String>(arena, 1024 * 1024 * 4);
	std::unordered_set<std::string> stdhs;
	auto time_begin = std::chrono::high_resolution_clock::now();
	constexpr u64 NUM_INSERTS = 1024 * 32;
	for (u64 i = 0; i < NUM_INSERTS; i++) {
		if (i % 1000000 == 0) {
			printf("Inserted %llu items into hash set\n", i);
		}
		hs.insert(random_string(arena, 1024));
	}
	auto time_end = std::chrono::high_resolution_clock::now();
	LUMEN_INFO("Time taken for lm::hash_set: %f seconds\n",
			   std::chrono::duration<double>(time_end - time_begin).count());

	time_begin = std::chrono::high_resolution_clock::now();
	for (u64 i = 0; i < NUM_INSERTS; i++) {
		if (i % 1000000 == 0) {
			printf("Inserted %llu items into hash set\n", i);
		}
		stdhs.insert(random_std_string(1024));
	}
	time_end = std::chrono::high_resolution_clock::now();

	LUMEN_INFO("Time taken for std::unordered_set: %f seconds\n",
			   std::chrono::duration<double>(time_end - time_begin).count());
	LUMEN_TRACE("----Hash Set Test END----");
}

void scratch_arena_test() {
	LUMEN_TRACE("----Scratch Arena Test----");
	lm::Arena* arena = lm::arena_create(MB(1));
	lm::FixedArray<i32> arr = lm::fixed_array_create<i32>(arena, 512);
	{
		lm::ScratchArena scratch = arena;
		lm::FixedArray<i32> arr2 = lm::fixed_array_create<i32>(scratch.arena, 256);
	}
	LUMEN_TRACE("----Scratch Arena Test End----");
}

void hm_test() {
	LUMEN_TRACE("----Hash Map Test----");
	lm::Arena* arena = lm::arena_create(KB(1));

	auto hm = lm::hash_map_create<u32, u32>(arena);
	for (u32 i = 0; i < 4; i++) {
		hm.insert(i, rand() & U32_MAX);
	}

	constexpr u32 HM_SIZE = 1024 * 1024;
	for (u32 i = 0; i < HM_SIZE; i++) {
		hm.insert(i, rand() & U32_MAX);
	}
	auto hs = lm::hash_set_create<u32>(arena);
	for (u32 i = 0; i < 4; i++) {
		hs.insert(i);
	}

	// for(const auto& kv: hm) {
	// 	LUMEN_INFO("%d - %d\n", kv.key, kv.value);
	// }
	for (const lm::HashMapEntry<u32, lm::Empty>& k : hs) {
		LUMEN_INFO("%d", k.key);
	}
	LUMEN_TRACE("----Hash Map Test End----");
}

i32 main(i32 argc, char* argv[]) {
	hm_test();
	return 0;
	hash_set_u64_test();
	hash_set_str_test();
	scratch_arena_test();
	lm::Arena* arena = lm::arena_create(GB(1), MB(1));

	lm::Array<i32> arr = lm::array_create<i32>(arena);

	for (i32 i = 0; i < 100'000'000; i++) {
		arr.push_back(i);
	}

	auto hm = lm::hash_map_create<i32, u64>(arena);

	hm.insert(1, 100);
	hm.insert(2, 200);
	hm.insert(2, 400);
	hm.insert(682, 800);
	hm.remove(2);

	auto* entry = hm.find(682);
	assert(entry != nullptr);
	auto* entry2 = hm.find(3);
	assert(entry2 == nullptr);
	auto* entry4 = hm.find(2);
	assert(entry4 == nullptr);

	for (lm::HashMapEntry<i32, u64>& e : hm) {
		LUMEN_INFO("HM Entry size = %d", sizeof(e));
		LUMEN_INFO("HashMap entry: key = %d, value = %llu", e.key, e.value);
	}

	auto hs = lm::hash_set_create<u64>(arena);
	hs.insert(24);
	hs.insert(24);
	hs.insert(541);
	hs.insert(60);
	for (lm::HashMapEntry<u64, lm::Empty>& e : hs) {
		LUMEN_INFO("HS Entry size = %d", sizeof(e));
		LUMEN_INFO("HashSet entry: key = %llu", e.key);
	}

	lm::String result = lm::str_from_u64(arena, 62832387);
	lm::String result2 = lm::str_from_s64(arena, -62832387);
	lm::String result3 = lm::str_from_f64(arena, 1421.363);

	LUMEN_INFO("CSTR Literal: %s", CSTR("Test").data);
	LUMEN_INFO("Result %s", lm::str_to_cstr(arena, result).data);
	LUMEN_INFO("Result2 %s", lm::str_to_cstr(arena, result2).data);
	LUMEN_INFO("Result3 %s", lm::str_to_cstr(arena, result3).data);

	LUMEN_INFO("U64 from str: %llu", lm::u64_from_str(CSTR("123456789")));
	LUMEN_INFO("U64 from str: %llu", lm::u64_from_str(CSTR("  asd  123456789asd")));
	LUMEN_INFO("U64 from str2: %llu", lm::u64_from_str(CSTR("   18446744073709551616")));
	LUMEN_INFO("S64 from str: %lld", lm::s64_from_str(CSTR("  -  123456789asd")));

	LUMEN_INFO("%f", f64_from_str(CSTR("-1.32e-1")));
	LUMEN_INFO("%f", f64_from_str(CSTR("1.2423")));
	LUMEN_INFO("%f", f64_from_str(CSTR("-1.2423")));
	LUMEN_INFO("%f", f64_from_str(CSTR(".2423")));
	LUMEN_INFO("%f", f64_from_str(CSTR("1361763176537161637")));
	LUMEN_INFO("%f", f32_from_str(CSTR("1432.34")));
	LUMEN_INFO("%f", f64_from_str(CSTR("-0.9814223")));

	os::FileHandle file_handle = os::file_open(CSTR("scenes/cornell_box/path.scene"), os::AccessFlag_Read);
	if (file_handle == 0) {
		LUMEN_ERROR("Failed to open file");
		return -1;
	}
	os::FileProperties props = os::file_properties(file_handle);
	LUMEN_INFO("File size: %llu, created: %llu, modified: %llu", props.size, props.created, props.modified);

	lm::String file_content = lm::str_reserve(arena, props.size + 1);

	u64 bytes_read = os::file_read(file_handle, file_content.data);
	file_content.data[props.size] = '\0';

	LUMEN_INFO("Bytes read: %llu - File content: %s", bytes_read, file_content.data);

	// __debugbreak();
}
#endif
