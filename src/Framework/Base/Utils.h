#pragma once

namespace util {

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

template <typename A, typename B>
struct is_same {
	static constexpr bool value = false;
};
template <typename A>
struct is_same<A, A> {
	static constexpr bool value = true;
};

template <typename T>
struct Slice {
	T* data = nullptr;
	u64 size = 0;
	Slice() = default;
	Slice(T* data, u64 size) : data(data), size(size) {}
	T& operator[](u64 idx) { return data[idx]; }
	T& operator[](u64 idx) const { return data[idx]; }
	T* begin() { return data; }
	T* end() { return data + size; }

	const T* begin() const { return data; }
	const T* end() const { return data + size; }
	inline bool empty() const { return size == 0; }
};

}  // namespace util
