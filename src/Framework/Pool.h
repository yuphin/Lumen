#pragma once
#include "../LumenPCH.h"
#include "Handle.h"

namespace lumen {
template <typename T>
class Pool {
   public:
	Pool();
	T* get(Handle<T> handle);
	Handle<T> add(T&& value);
	void remove(Handle<T> handle);

   private:
	// type - generation counter
	std::vector<std::pair<T, uint32_t>> data;
	std::vector<size_t> free_list;
};

template <typename T>
Pool<T>::Pool() {
	data.reserve(32);
	free_list.reserve(32);
}

template <typename T>
T* Pool<T>::get(Handle<T> handle) {
	assert(handle.index < data.size());
	if (data[handle.index].second != handle.generation) {
		// Handle was replaced
		return nullptr;
	}
	return &data[handle.index].first;
}

template <typename T>
Handle<T> Pool<T>::add(T&& value) {
	if (free_list.empty()) {
		data.push_back(std::make_pair(std::move(value), 1));
		return Handle<T>(data.size() - 1, 1);
	}
	size_t index = free_list.back();
	free_list.pop_back();
    assert(data[index].second > 1);
	data[index] = std::make_pair(std::move(value), data[index].second);
	return Handle<T>(index, data[index].second);
}

template <typename T>
void Pool<T>::remove(Handle<T> handle) {
    ++data[handle.index].second;
	free_list.push_back(handle.index);
}
}  // namespace lumen