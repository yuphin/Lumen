#pragma once

namespace lm {
template <typename T, u64 N>
struct SmallArray {
	u64 count = 0;
	T data[N];

	void push_back(const T& val) {
		assert(count < N);
		data[count++] = val;
	}

	void push_back_move(T&& v) {
		assert(count < N);
		data[count++] = std::move(v);
	}

	T& push() {
		assert(count < N);
		return data[count++];
	}

	void pop_back() {
		assert(count > 0);
		--count;
	}

	inline bool empty() const { return count == 0; }
	inline constexpr u64 capacity() { return N; }
	T* begin() const { return data; }
	T* end() const { return data + count; }
	T* begin() { return data; }
	T* end() { return data + count; }
	void clear() { count = 0; }

	T& operator[](u64 index) {
		assert(index < count);
		return data[index];
	}
	T& operator[](u64 index) const {
		assert(index < count);
		return data[index];
	}

	T& back() {
		assert(count > 0);
		return data[count - 1];
	}
};
}  // namespace lm
