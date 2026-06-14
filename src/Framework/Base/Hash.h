#pragma once
#include "String.h"

namespace lm {
template <typename T, bool GROWABLE>
struct Array;

inline constexpr u64 HASH_INIT = 5381;

inline u64 sdbm_hash(const void* data, u64 size, u64 hash = HASH_INIT) {
	const u8* bytes = (const u8*)data;
	for (u64 i = 0; i < size; i++) {
		hash = bytes[i] + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

inline constexpr u64 FNV_64_PRIME = 0x100000001b3;
inline constexpr u64 FNV_64_OFFSET_BIAS = 0xcbf29ce484222325;

inline u64 fnv1a_hash(u64 val, u64 hash = FNV_64_OFFSET_BIAS) {
	hash ^= val;
	return hash * FNV_64_PRIME;
}

inline u64 fnv1a_hash(const void* data, u64 size, u64 hash = FNV_64_OFFSET_BIAS) {
	const u8* bytes = (const u8*)data;
	for (u64 i = 0; i < size; i++) {
		hash = fnv1a_hash(bytes[i], hash);
	}
	return hash;
}

inline u64 knuth_hash(u64 x) {
	constexpr u64 MULTIPLIER = 11400714819323198485ULL;
	return MULTIPLIER * x;
}

#define LM_INTEGER_HASH(TYPE) \
	inline u64 hash(TYPE value) { return knuth_hash((u64)value ^ HASH_INIT); }

LM_INTEGER_HASH(bool)
LM_INTEGER_HASH(char)
LM_INTEGER_HASH(wchar_t)
LM_INTEGER_HASH(char8_t)
LM_INTEGER_HASH(char16_t)
LM_INTEGER_HASH(char32_t)
LM_INTEGER_HASH(signed char)
LM_INTEGER_HASH(unsigned char)
LM_INTEGER_HASH(short)
LM_INTEGER_HASH(unsigned short)
LM_INTEGER_HASH(int)
LM_INTEGER_HASH(unsigned int)
LM_INTEGER_HASH(long)
LM_INTEGER_HASH(unsigned long)
LM_INTEGER_HASH(long long)
LM_INTEGER_HASH(unsigned long long)

#undef LM_INTEGER_HASH

inline u64 hash(f32 value) { return sdbm_hash(&value, sizeof(value), HASH_INIT); }
inline u64 hash(f64 value) { return sdbm_hash(&value, sizeof(value), HASH_INIT); }
inline u64 hash(long double value) { return sdbm_hash(&value, sizeof(value), HASH_INIT); }

template <typename T>
	requires(__is_enum(T))
inline u64 hash(T value) {
	return knuth_hash((u64)value ^ HASH_INIT);
}

template <typename T>
inline u64 hash(T* value) {
	return knuth_hash(reinterpret_cast<u64>(value) ^ HASH_INIT);
}

inline u64 hash(const String& string) {
	return fnv1a_hash(string.data, string.size, HASH_INIT);
}

template <typename T, bool GROWABLE>
inline u64 hash(const Array<T, GROWABLE>& array) {
	return sdbm_hash(array.data, array.size * sizeof(T), HASH_INIT);
}

template <typename T>
inline void hash_combine(u64& seed, const T& value) {
	seed ^= hash(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T, typename U, typename... Rest>
inline void hash_combine(u64& seed, const T& value, const U& next, const Rest&... rest) {
	hash_combine(seed, value);
	hash_combine(seed, next, rest...);
}

template <typename T>
inline u64 default_hash(const T& value) {
	return hash(value);
}
}  // namespace lm
