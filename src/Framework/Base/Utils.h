#pragma once

namespace util {

template<typename A, typename B> struct is_same { static constexpr bool value = false; };
template<typename A> struct is_same<A, A> { static constexpr bool value = true; };

inline constexpr u64 align_pow2(u64 x, u64 align) { return (x + align - 1) & ~(align - 1); }

template <typename T>
inline constexpr T next_pow2(T x) {
	if (x == 0) return 1;
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

template <class T>
inline constexpr T align_up_pow2(T x, u64 a) noexcept {
	return T((x + (T(a) - 1)) & ~T(a - 1));
}

template <typename T, typename... Rest>
inline void hash_combine(u64& seed, const T& v) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T, typename... Rest>
inline void hash_combine(u64& seed, const T& v, Rest... rest) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	hash_combine(seed, rest...);
}

template <typename T>
struct Slice {
	T* data = nullptr;
	u64 size = 0;
	Slice() = default;
	Slice(T* data, u64 size) : data(data), size(size) {}
	T& operator[](u64 idx) { return data[idx]; }
	T& begin() { return data[0]; }
	T& end() { return data[size - 1]; }
	inline bool empty() const { return size == 0; }
};

}  // namespace util