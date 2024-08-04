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