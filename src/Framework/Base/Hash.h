#pragma once
namespace lm {

inline constexpr u64 HASH_INIT = 5381;

static inline uint64_t sdbm_hash(void* data, u64 size, uint64_t hash = HASH_INIT) {
	u8* bytes = (u8*)data;
	for (u64 i = 0; i < size; i++) {
		hash = bytes[i] + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

static inline u64 FNV_64_PRIME = 0x100000001b3;
static inline u64 FNV_64_OFFSET_BIAS = 0xcbf29ce484222325;

static inline uint64_t fnv1a_hash(uint64_t val, uint64_t hash = FNV_64_OFFSET_BIAS) {
	hash ^= val;
	return hash * FNV_64_PRIME;
}

static inline uint64_t fnv1a_hash(void* data, u64 size, uint64_t hash = FNV_64_OFFSET_BIAS) {
	u8* bytes = (u8*)data;
	for (u64 i = 0; i < size; i++) {
		hash = fnv1a_hash(bytes[i], hash);
	}
	return hash;
}

static inline uint64_t knuth_hash(uint64_t x) {
	constexpr uint64_t MULTIPLIER = 11400714819323198485ULL;
	return MULTIPLIER * x;
}
}  // namespace lm
