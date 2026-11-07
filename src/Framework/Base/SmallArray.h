#pragma once

#include "Framework/Base/Utils.h"
namespace lm {
template <typename T, u64 N>
struct SmallArray {
	u64 size = 0;
	// This avoids initialization and redundant memsets during construction
	// initialization is now done in push or resize
	union {
		T data[N];
	};

	// Needed to avoid zero initialization
	constexpr SmallArray() {}

	constexpr SmallArray(std::initializer_list<T> init) {
		assert(init.size() <= N);
		for (const T& v : init) {
			data[size++] = v;
		}
	}

	void push_back(const T& val) {
		assert(size < N);
		data[size++] = val;
	}

	void push_back_move(T&& v) {
		assert(size < N);
		data[size++] = std::move(v);
	}

	template <typename... Args>
	T& emplace_back(Args&&... args) {
		assert(size < N);
		T& new_element = data[size];
		new (&new_element) T(std::forward<Args>(args)...);
		size++;
		return new_element;
	}

	T& push() {
		assert(size < N);
		new (&data[size]) T{};
		return data[size++];
	}

	void pop_back() {
		assert(size > 0);
		--size;
	}

	void resize(u64 new_size) {
		assert(new_size <= N);
		for (u64 i = size; i < new_size; i++) {
			new (&data[i]) T{};
		}
		size = new_size;
	}

	void erase_unordered(u64 idx) {
		assert(idx < size);
		data[idx] = data[size - 1];
		size--;
	}

	inline bool empty() const { return size == 0; }
	inline constexpr u64 capacity() { return N; }
	const T* begin() const { return data; }
	const T* end() const { return data + size; }
	T* begin() { return data; }
	T* end() { return data + size; }
	void clear() { size = 0; }

	T& operator[](u64 index) {
		assert(index < size);
		return data[index];
	}
	const T& operator[](u64 index) const {
		assert(index < size);
		return data[index];
	}

	T& back() {
		assert(size > 0);
		return data[size - 1];
	}
	util::Slice<T> to_slice() const { return util::Slice(&data[0], size); }
	util::Slice<T> to_slice() { return util::Slice(&data[0], size); }
};
template <typename T, typename... U>
SmallArray(T, U...) -> SmallArray<T, 1 + sizeof...(U)>;
}  // namespace lm
