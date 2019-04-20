#pragma once
#include <functional>
#include "vulkan\vulkan_core.h"


template <typename T>
class VScopedPtr {
public:
	VScopedPtr() : VScopedPtr([](T, VkAllocationCallbacks*) {}) {}

	VScopedPtr(std::function<void(T, VkAllocationCallbacks*)> deletef) {
		this->deleter = [&](T obj) { deletef(obj, nullptr); };
	}

	VScopedPtr(const VScopedPtr<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) {
		this->deleter = [&](T obj) { deletef(instance, obj, nullptr); };
	}

	VScopedPtr(const VScopedPtr<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) {
		this->deleter = [&](T obj) { deletef(device, obj, nullptr); };
	}

	~VScopedPtr() {
		cleanup();
	}

	const T* operator &() const {
		return &object;
	}

	T* replace() {
		cleanup();
		return &object;
	}

	operator T() const {
		return object;
	}

	void operator=(T rhs) {
		cleanup();
		object = rhs;
	}

	template<typename V>
	bool operator==(V rhs) {
		return object == T(rhs);
	}

private:
	T object = VK_NULL_HANDLE ;
	std::function<void(T)> deleter;

	void cleanup() {
		if (object != VK_NULL_HANDLE) {
			deleter(object);
		}
		object = VK_NULL_HANDLE;
	}
};