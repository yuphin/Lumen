#pragma once
#include <cmath>

namespace lm {

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

template <typename T>
struct Vec4;

template <typename T>
struct Vec2 {
	union {
		struct {
			T x, y;
		};
		struct {
			T r, g;
		};
	};

	constexpr Vec2() : x(0), y(0) {}
	constexpr Vec2(T value) : x(value), y(value) {}
	template <typename A, typename B>
	constexpr Vec2(A x, B y) : x((T)x), y((T)y) {}
	template <typename U>
	constexpr Vec2(const Vec2<U>& v) : x((T)v.x), y((T)v.y) {}

	static constexpr u32 length() { return 2; }
	T& operator[](u32 i) { return (&x)[i]; }
	const T& operator[](u32 i) const { return (&x)[i]; }
};

template <typename T>
struct Vec3 {
	union {
		struct {
			T x, y, z;
		};
		struct {
			T r, g, b;
		};
	};

	constexpr Vec3() : x(0), y(0), z(0) {}
	constexpr Vec3(T value) : x(value), y(value), z(value) {}
	template <typename A, typename B, typename C>
	constexpr Vec3(A x, B y, C z) : x((T)x), y((T)y), z((T)z) {}
	template <typename U>
	constexpr Vec3(const Vec3<U>& v) : x((T)v.x), y((T)v.y), z((T)v.z) {}
	template <typename U>
	constexpr Vec3(const Vec4<U>& v) : x((T)v.x), y((T)v.y), z((T)v.z) {}

	static constexpr u32 length() { return 3; }
	T& operator[](u32 i) { return (&x)[i]; }
	const T& operator[](u32 i) const { return (&x)[i]; }
};

template <typename T>
struct Vec4 {
	union {
		struct {
			T x, y, z, w;
		};
		struct {
			T r, g, b, a;
		};
	};

	constexpr Vec4() : x(0), y(0), z(0), w(0) {}
	constexpr Vec4(T value) : x(value), y(value), z(value), w(value) {}
	template <typename A, typename B, typename C, typename D>
	constexpr Vec4(A x, B y, C z, D w) : x((T)x), y((T)y), z((T)z), w((T)w) {}
	constexpr Vec4(Vec3<T> v, T w) : x(v.x), y(v.y), z(v.z), w(w) {}
	template <typename U>
	constexpr Vec4(const Vec4<U>& v) : x((T)v.x), y((T)v.y), z((T)v.z), w((T)v.w) {}

	static constexpr u32 length() { return 4; }
	T& operator[](u32 i) { return (&x)[i]; }
	const T& operator[](u32 i) const { return (&x)[i]; }
};

using vec2 = Vec2<f32>;
using vec3 = Vec3<f32>;
using vec4 = Vec4<f32>;
using ivec2 = Vec2<i32>;
using ivec3 = Vec3<i32>;
using uvec2 = Vec2<u32>;
using uvec4 = Vec4<u32>;
using bvec3 = Vec3<bool>;

#define LM_VEC_BINARY_OP(TYPE, OP)                                            \
	template <typename T>                                                     \
	constexpr TYPE<T> operator OP(const TYPE<T>& a, const TYPE<T>& b) {       \
		TYPE<T> result;                                                       \
		for (u32 i = 0; i < TYPE<T>::length(); ++i) result[i] = a[i] OP b[i]; \
		return result;                                                        \
	}                                                                         \
	template <typename T>                                                     \
	constexpr TYPE<T> operator OP(const TYPE<T>& a, T b) {                    \
		TYPE<T> result;                                                       \
		for (u32 i = 0; i < TYPE<T>::length(); ++i) result[i] = a[i] OP b;    \
		return result;                                                        \
	}                                                                         \
	template <typename T>                                                     \
	constexpr TYPE<T> operator OP(T a, const TYPE<T>& b) {                    \
		TYPE<T> result;                                                       \
		for (u32 i = 0; i < TYPE<T>::length(); ++i) result[i] = a OP b[i];    \
		return result;                                                        \
	}

LM_VEC_BINARY_OP(Vec2, +)
LM_VEC_BINARY_OP(Vec2, -)
LM_VEC_BINARY_OP(Vec2, *)
LM_VEC_BINARY_OP(Vec2, /)
LM_VEC_BINARY_OP(Vec3, +)
LM_VEC_BINARY_OP(Vec3, -)
LM_VEC_BINARY_OP(Vec3, *)
LM_VEC_BINARY_OP(Vec3, /)
LM_VEC_BINARY_OP(Vec4, +)
LM_VEC_BINARY_OP(Vec4, -)
LM_VEC_BINARY_OP(Vec4, *)
LM_VEC_BINARY_OP(Vec4, /)
#undef LM_VEC_BINARY_OP

#define LM_VEC_COMPOUND_OP(TYPE, OP)                               \
	template <typename T>                                          \
	constexpr TYPE<T>& operator OP(TYPE<T>& a, const TYPE<T>& b) { \
		for (u32 i = 0; i < TYPE<T>::length(); ++i) a[i] OP b[i];  \
		return a;                                                  \
	}                                                              \
	template <typename T>                                          \
	constexpr TYPE<T>& operator OP(TYPE<T>& a, T b) {              \
		for (u32 i = 0; i < TYPE<T>::length(); ++i) a[i] OP b;     \
		return a;                                                  \
	}

LM_VEC_COMPOUND_OP(Vec2, +=)
LM_VEC_COMPOUND_OP(Vec2, -=)
LM_VEC_COMPOUND_OP(Vec2, *=)
LM_VEC_COMPOUND_OP(Vec2, /=)
LM_VEC_COMPOUND_OP(Vec3, +=)
LM_VEC_COMPOUND_OP(Vec3, -=)
LM_VEC_COMPOUND_OP(Vec3, *=)
LM_VEC_COMPOUND_OP(Vec3, /=)
LM_VEC_COMPOUND_OP(Vec4, +=)
LM_VEC_COMPOUND_OP(Vec4, -=)
LM_VEC_COMPOUND_OP(Vec4, *=)
LM_VEC_COMPOUND_OP(Vec4, /=)
#undef LM_VEC_COMPOUND_OP

#define LM_VEC_UNARY_AND_COMPARE(TYPE)                                 \
	template <typename T>                                              \
	constexpr TYPE<T> operator-(const TYPE<T>& v) {                    \
		TYPE<T> result;                                                \
		for (u32 i = 0; i < TYPE<T>::length(); ++i) result[i] = -v[i]; \
		return result;                                                 \
	}                                                                  \
	template <typename T>                                              \
	constexpr bool operator==(const TYPE<T>& a, const TYPE<T>& b) {    \
		for (u32 i = 0; i < TYPE<T>::length(); ++i)                    \
			if (a[i] != b[i]) return false;                            \
		return true;                                                   \
	}                                                                  \
	template <typename T>                                              \
	constexpr bool operator!=(const TYPE<T>& a, const TYPE<T>& b) {    \
		return !(a == b);                                              \
	}

LM_VEC_UNARY_AND_COMPARE(Vec2)
LM_VEC_UNARY_AND_COMPARE(Vec3)
LM_VEC_UNARY_AND_COMPARE(Vec4)
#undef LM_VEC_UNARY_AND_COMPARE

struct mat4 {
	vec4 columns[4];

	constexpr mat4(f32 diagonal = 0.0f)
		: columns{{diagonal, 0, 0, 0}, {0, diagonal, 0, 0}, {0, 0, diagonal, 0}, {0, 0, 0, diagonal}} {}

	vec4& operator[](u32 i) { return columns[i]; }
	const vec4& operator[](u32 i) const { return columns[i]; }
};

struct quat {
	f32 w = 1.0f;
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 z = 0.0f;
};

template <typename T>
constexpr T min(T a, T b) {
	return a < b ? a : b;
}
template <typename T>
constexpr T max(T a, T b) {
	return a > b ? a : b;
}
template <typename T>
constexpr T clamp(T value, T low, T high) {
	return min(max(value, low), high);
}
template <typename T>
constexpr Vec3<T> min(const Vec3<T>& a, const Vec3<T>& b) {
	return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}
template <typename T>
constexpr Vec3<T> max(const Vec3<T>& a, const Vec3<T>& b) {
	return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}
template <typename T>
constexpr Vec3<T> clamp(const Vec3<T>& v, T low, T high) {
	return {clamp(v.x, low, high), clamp(v.y, low, high), clamp(v.z, low, high)};
}
template <typename T>
constexpr Vec3<T> max(const Vec3<T>& v, T value) {
	return {max(v.x, value), max(v.y, value), max(v.z, value)};
}

inline f32 sqrt(f32 value) { return sqrtf(value); }
inline vec3 sqrt(const vec3& v) { return {sqrtf(v.x), sqrtf(v.y), sqrtf(v.z)}; }
inline f32 round(f32 value) { return roundf(value); }
inline f32 ceil(f32 value) { return ceilf(value); }
inline f64 ceil(f64 value) { return ceill(value); }
inline f32 floor(f32 value) { return floorf(value); }
inline f64 floor(f64 value) { return floorl(value); }
inline f32 log2(f32 value) { return log2f(value); }
inline f64 log2(f64 value) { return log2l(value); }
inline u32 log2(u32 value) { return 31 - count_leading_zeros(value); }
inline u64 log2(u64 value) { return 63 - count_leading_zeros(value); }
inline vec3 fmod(const vec3& a, const vec3& b) { return {fmodf(a.x, b.x), fmodf(a.y, b.y), fmodf(a.z, b.z)}; }

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

template <typename T = f32>
constexpr T pi() {
	return (T)3.14159265358979323846;
}
inline f32 radians(f32 degrees) { return degrees * (pi<f32>() / 180.0f); }
inline vec3 radians(const vec3& degrees) { return degrees * (pi<f32>() / 180.0f); }
inline vec3 degrees(const vec3& radians_value) { return radians_value * (180.0f / pi<f32>()); }

inline f32 dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline f32 length(const vec3& v) { return sqrtf(dot(v, v)); }
inline vec3 normalize(const vec3& v) {
	f32 len = length(v);
	return len > 0.0f ? v / len : vec3(0.0f);
}
inline vec3 cross(const vec3& a, const vec3& b) {
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline bvec3 greaterThan(const vec3& a, const vec3& b) { return {a.x > b.x, a.y > b.y, a.z > b.z}; }
inline bool any(const bvec3& v) { return v.x || v.y || v.z; }

inline vec4 operator*(const mat4& m, const vec4& v) { return m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3] * v.w; }
inline mat4 operator*(const mat4& a, const mat4& b) {
	mat4 result;
	for (u32 i = 0; i < 4; ++i) result[i] = a * b[i];
	return result;
}
inline mat4 transpose(const mat4& m) {
	mat4 result;
	for (u32 column = 0; column < 4; ++column)
		for (u32 row = 0; row < 4; ++row) result[column][row] = m[row][column];
	return result;
}
inline mat4 translate(const mat4& m, const vec3& v) {
	mat4 result = m;
	result[3] = m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3];
	return result;
}
inline mat4 scale(const mat4& m, const vec3& v) {
	mat4 result = m;
	result[0] *= v.x;
	result[1] *= v.y;
	result[2] *= v.z;
	return result;
}
inline mat4 rotate(const mat4& m, f32 angle, const vec3& raw_axis) {
	vec3 axis = normalize(raw_axis);
	f32 c = cosf(angle);
	f32 s = sinf(angle);
	f32 t = 1.0f - c;
	mat4 rotation(1.0f);
	rotation[0] = {c + axis.x * axis.x * t, axis.y * axis.x * t + axis.z * s, axis.z * axis.x * t - axis.y * s, 0};
	rotation[1] = {axis.x * axis.y * t - axis.z * s, c + axis.y * axis.y * t, axis.z * axis.y * t + axis.x * s, 0};
	rotation[2] = {axis.x * axis.z * t + axis.y * s, axis.y * axis.z * t - axis.x * s, c + axis.z * axis.z * t, 0};
	return m * rotation;
}

inline mat4 inverse(const mat4& m) {
	f32 augmented[4][8] = {};
	for (u32 row = 0; row < 4; ++row) {
		for (u32 column = 0; column < 4; ++column) augmented[row][column] = m[column][row];
		augmented[row][row + 4] = 1.0f;
	}
	for (u32 pivot = 0; pivot < 4; ++pivot) {
		u32 best = pivot;
		for (u32 row = pivot + 1; row < 4; ++row)
			if (fabsf(augmented[row][pivot]) > fabsf(augmented[best][pivot])) best = row;
		if (best != pivot)
			for (u32 column = 0; column < 8; ++column) {
				f32 temp = augmented[pivot][column];
				augmented[pivot][column] = augmented[best][column];
				augmented[best][column] = temp;
			}
		f32 divisor = augmented[pivot][pivot];
		LUMEN_ASSERT(fabsf(divisor) > 1e-8f, "Cannot invert singular matrix");
		if (fabsf(divisor) <= 1e-8f) return mat4(1.0f);
		for (u32 column = 0; column < 8; ++column) augmented[pivot][column] /= divisor;
		for (u32 row = 0; row < 4; ++row) {
			if (row == pivot) continue;
			f32 factor = augmented[row][pivot];
			for (u32 column = 0; column < 8; ++column) augmented[row][column] -= factor * augmented[pivot][column];
		}
	}
	mat4 result;
	for (u32 row = 0; row < 4; ++row)
		for (u32 column = 0; column < 4; ++column) result[column][row] = augmented[row][column + 4];
	return result;
}

inline quat angle_axis(f32 angle, const vec3& axis) {
	f32 half = angle * 0.5f;
	f32 s = sinf(half);
	return {cosf(half), axis.x * s, axis.y * s, axis.z * s};
}
inline mat4 mat4_cast(const quat& q) {
	f32 xx = q.x * q.x;
	f32 yy = q.y * q.y;
	f32 zz = q.z * q.z;
	f32 xy = q.x * q.y;
	f32 xz = q.x * q.z;
	f32 yz = q.y * q.z;
	f32 wx = q.w * q.x;
	f32 wy = q.w * q.y;
	f32 wz = q.w * q.z;
	mat4 result(1.0f);
	result[0] = {1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0};
	result[1] = {2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0};
	result[2] = {2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0};
	return result;
}
inline mat4 to_mat4(const quat& q) { return mat4_cast(q); }

inline quat quat_from_mat4(const mat4& m) {
	quat q;
	f32 trace = m[0][0] + m[1][1] + m[2][2];
	if (trace > 0.0f) {
		f32 s = sqrtf(trace + 1.0f) * 2.0f;
		q.w = 0.25f * s;
		q.x = (m[1][2] - m[2][1]) / s;
		q.y = (m[2][0] - m[0][2]) / s;
		q.z = (m[0][1] - m[1][0]) / s;
	} else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
		f32 s = sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f;
		q.w = (m[1][2] - m[2][1]) / s;
		q.x = 0.25f * s;
		q.y = (m[1][0] + m[0][1]) / s;
		q.z = (m[2][0] + m[0][2]) / s;
	} else if (m[1][1] > m[2][2]) {
		f32 s = sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f;
		q.w = (m[2][0] - m[0][2]) / s;
		q.x = (m[1][0] + m[0][1]) / s;
		q.y = 0.25f * s;
		q.z = (m[2][1] + m[1][2]) / s;
	} else {
		f32 s = sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f;
		q.w = (m[0][1] - m[1][0]) / s;
		q.x = (m[2][0] + m[0][2]) / s;
		q.y = (m[2][1] + m[1][2]) / s;
		q.z = 0.25f * s;
	}
	return q;
}

inline bool decompose(const mat4& matrix, vec3& out_scale, quat& out_rotation, vec3& out_translation, vec3& out_skew,
					  vec4& out_perspective) {
	if (fabsf(matrix[3][3]) <= 1e-8f) return false;

	mat4 local = matrix;
	for (u32 column = 0; column < 4; ++column) local[column] /= local[3][3];

	mat4 perspective_matrix = local;
	perspective_matrix[0][3] = 0.0f;
	perspective_matrix[1][3] = 0.0f;
	perspective_matrix[2][3] = 0.0f;
	perspective_matrix[3][3] = 1.0f;
	if (fabsf(local[0][3]) > 1e-8f || fabsf(local[1][3]) > 1e-8f || fabsf(local[2][3]) > 1e-8f) {
		out_perspective =
			transpose(inverse(perspective_matrix)) * vec4(local[0][3], local[1][3], local[2][3], local[3][3]);
		local[0][3] = local[1][3] = local[2][3] = 0.0f;
		local[3][3] = 1.0f;
	} else {
		out_perspective = vec4(0, 0, 0, 1);
	}

	out_translation = vec3(local[3]);
	local[3] = vec4(0, 0, 0, local[3].w);

	vec3 basis[3] = {vec3(local[0]), vec3(local[1]), vec3(local[2])};
	out_scale.x = length(basis[0]);
	if (out_scale.x <= 1e-8f) return false;
	basis[0] /= out_scale.x;

	out_skew.z = dot(basis[0], basis[1]);
	basis[1] -= basis[0] * out_skew.z;
	out_scale.y = length(basis[1]);
	if (out_scale.y <= 1e-8f) return false;
	basis[1] /= out_scale.y;
	out_skew.z /= out_scale.y;

	out_skew.y = dot(basis[0], basis[2]);
	basis[2] -= basis[0] * out_skew.y;
	out_skew.x = dot(basis[1], basis[2]);
	basis[2] -= basis[1] * out_skew.x;
	out_scale.z = length(basis[2]);
	if (out_scale.z <= 1e-8f) return false;
	basis[2] /= out_scale.z;
	out_skew.y /= out_scale.z;
	out_skew.x /= out_scale.z;

	if (dot(basis[0], cross(basis[1], basis[2])) < 0.0f) {
		out_scale = -out_scale;
		basis[0] = -basis[0];
		basis[1] = -basis[1];
		basis[2] = -basis[2];
	}

	mat4 rotation(1.0f);
	rotation[0] = vec4(basis[0], 0);
	rotation[1] = vec4(basis[1], 0);
	rotation[2] = vec4(basis[2], 0);
	out_rotation = quat_from_mat4(rotation);
	return true;
}

inline void extract_euler_angle_xyz(const mat4& m, f32& x, f32& y, f32& z) {
	f32 x_angle = atan2f(m[2][1], m[2][2]);
	f32 cos_y = sqrtf(m[0][0] * m[0][0] + m[1][0] * m[1][0]);
	f32 y_angle = atan2f(-m[2][0], cos_y);
	f32 sin_x = sinf(x_angle);
	f32 cos_x = cosf(x_angle);
	f32 z_angle = atan2f(sin_x * m[0][2] - cos_x * m[0][1], cos_x * m[1][1] - sin_x * m[1][2]);
	x = -x_angle;
	y = -y_angle;
	z = -z_angle;
}

static_assert(sizeof(vec2) == 8);
static_assert(sizeof(vec3) == 12);
static_assert(sizeof(vec4) == 16);
static_assert(sizeof(mat4) == 64);

}  // namespace lm
