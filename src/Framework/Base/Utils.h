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

template <typename T>
inline constexpr T div_ceil(T x, T y) {
	return (x + y - 1) / y;
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
	T& operator[](u64 idx) const { return data[idx]; }
	T* begin() { return data; }
	T* end() { return data + size; }

	const T* begin() const { return data; }
	const T* end() const { return data + size; }
	inline bool empty() const { return size == 0; }
};

inline u32 count_leading_zeros32(u32 value) {
#if defined(_MSC_VER)
	unsigned long index;
	if (_BitScanReverse(&index, value)) {
		return 31 - (u32)index;
	}
	return 32;	// Value is 0
#elif defined(__GNUC__) || defined(__clang__)
	if (value == 0) return 32;
	return (u32)__builtin_clz(value);
#else
	if (value == 0) return 32;
	u32 count = 0;
	while ((value & 0x80000000) == 0) {
		count++;
		value <<= 1;
	}
	return count;
#endif
}

inline u64 count_leading_zeros64(u64 value) {
#if defined(_MSC_VER)
	unsigned long index;
#if defined(_WIN64)
	if (_BitScanReverse64(&index, value)) {
		return 63 - (u64)index;
	}
#else
	if ((u32)(value >> 32) != 0) {
		if (_BitScanReverse(&index, (u32)(value >> 32))) return 31 - (u64)index;
	} else if ((u32)value != 0) {
		if (_BitScanReverse(&index, (u32)value)) return 63 - (u64)index;
	}
#endif
	return 64;
#elif defined(__GNUC__) || defined(__clang__)
	if (value == 0) return 64;
	return (u64)__builtin_clzll(value);
#else
	if (value == 0) return 64;
	u64 count = 0;
	while ((value & 0x8000000000000000ull) == 0) {
		count++;
		value <<= 1;
	}
	return count;
#endif
}

template <typename T>
inline u32 count_leading_zeros(T value) {
    static_assert(sizeof(T) <= 8, "Type too large for count_leading_zeros");

    if constexpr (sizeof(T) <= 4) {
        // The subtraction safely corrects the zero-padding for u8 and u16.
        return count_leading_zeros32((u32)value) - (32 - (sizeof(T) * 8));
    } else {
        return (u32)count_leading_zeros64((u64)value);
    }
}

}  // namespace util