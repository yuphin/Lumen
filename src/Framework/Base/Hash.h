#pragma once
namespace lm {
inline constexpr u64 HASH_INIT = 5381;

static inline u64 sdbm_hash(void* data, u64 size, u64 hash = HASH_INIT) {
	u8* bytes = (u8*)data;
	for (u64 i = 0; i < size; i++) {
		hash = bytes[i] + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

static inline u64 FNV_64_PRIME = 0x100000001b3;
static inline u64 FNV_64_OFFSET_BIAS = 0xcbf29ce484222325;

static inline u64 fnv1a_hash(u64 val, u64 hash = FNV_64_OFFSET_BIAS) {
	hash ^= val;
	return hash * FNV_64_PRIME;
}

static inline u64 fnv1a_hash(void* data, u64 size, u64 hash = FNV_64_OFFSET_BIAS) {
	u8* bytes = (u8*)data;
	for (u64 i = 0; i < size; i++) {
		hash = fnv1a_hash(bytes[i], hash);
	}
	return hash;
}

static inline u64 knuth_hash(u64 x) {
	constexpr u64 MULTIPLIER = 11400714819323198485ULL;
	return MULTIPLIER * x;
}

template <typename T>
static inline u64 default_hash(const T& x) {
	if constexpr (std::is_integral_v<T> || std::is_enum_v<T> || std::is_pointer_v<T> || std::is_floating_point_v<T>) {
		if constexpr (std::is_floating_point_v<T>) {
			return sdbm_hash((void*)&x, sizeof(T), HASH_INIT);
		} else if constexpr (std::is_pointer_v<T>) {
			return knuth_hash(static_cast<u64>(reinterpret_cast<std::uintptr_t>(x)) ^ HASH_INIT);
		} else {
			return knuth_hash(static_cast<u64>(x) ^ HASH_INIT);
		}
	} else if constexpr (std::is_same_v<T, lm::String>) {
		return fnv1a_hash((void*)x.data, x.size, HASH_INIT);
	} else {
		static_assert(false, "default_hash: Unsupported type for hashing");
		return 0;
	}
}

template <typename T>
static inline u64 default_hash(const Array<T>& array) {
	return sdbm_hash((void*)array.data, array.size * sizeof(T), HASH_INIT);
}
}  // namespace lm
