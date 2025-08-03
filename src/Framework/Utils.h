#pragma once

#define KB(n) (((size_t)(n)) << 10)
#define MB(n) (((size_t)(n)) << 20)
#define GB(n) (((size_t)(n)) << 30)

#define DEFINE_ENUM_FLAGS(T)                                                                                 \
	inline constexpr T operator|(T Lhs, T Rhs) {                                                             \
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(Lhs) |                                  \
							  static_cast<std::underlying_type_t<T>>(Rhs));                                  \
	}                                                                                                        \
	inline constexpr T operator&(T Lhs, T Rhs) {                                                             \
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(Lhs) &                                  \
							  static_cast<std::underlying_type_t<T>>(Rhs));                                  \
	}                                                                                                        \
	inline constexpr T operator^(T Lhs, T Rhs) {                                                             \
		return static_cast<T>(static_cast<std::underlying_type_t<T>>(Lhs) ^                                  \
							  static_cast<std::underlying_type_t<T>>(Rhs));                                  \
	}                                                                                                        \
	inline constexpr T operator~(T E) { return static_cast<T>(~static_cast<std::underlying_type_t<T>>(E)); } \
	inline T& operator|=(T& Lhs, T Rhs) {                                                                    \
		return Lhs = static_cast<T>(static_cast<std::underlying_type_t<T>>(Lhs) |                            \
									static_cast<std::underlying_type_t<T>>(Lhs));                            \
	}                                                                                                        \
	inline T& operator&=(T& Lhs, T Rhs) {                                                                    \
		return Lhs = static_cast<T>(static_cast<std::underlying_type_t<T>>(Lhs) &                            \
									static_cast<std::underlying_type_t<T>>(Lhs));                            \
	}                                                                                                        \
	inline T& operator^=(T& Lhs, T Rhs) {                                                                    \
		return Lhs = static_cast<T>(static_cast<std::underlying_type_t<T>>(Lhs) ^                            \
									static_cast<std::underlying_type_t<T>>(Lhs));                            \
	}

namespace util {

inline constexpr size_t align_pow2(size_t x, size_t align) { return (x + align - 1) & ~(align - 1); }

inline constexpr uint32_t next_pow2(uint32_t x) {
	if (x == 0) return 1;
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

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