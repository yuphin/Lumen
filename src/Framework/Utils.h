#pragma once
namespace util {
template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	hash_combine(seed, rest...);
}

template <typename T>
struct Slice {
	T* data = nullptr;
	size_t size = 0;
	Slice() = default;
	Slice(T* data, size_t size) : data(data), size(size) {}
	T& operator[](size_t idx) { return data[idx]; }
	T& begin() { return data[0]; }
	T& end() { return data[size - 1]; }
	inline bool empty() const { return size == 0; }
};

}  // namespace util