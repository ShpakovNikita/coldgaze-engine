#pragma once

#include "Forwards.hpp"
#include "VScopedPtr.hpp"

class DevicePicker
{
public:
	DevicePicker(const VScopedPtr<VkInstance> &instance);
	~DevicePicker();

	int pick_best_device();
	int reset_instance(const VScopedPtr<VkInstance> &instance);

	VkPhysicalDevice get_device() const;

private:
	bool is_device_suitable(int score);
	int get_device_rating(VkPhysicalDevice device);

	VkPhysicalDevice _vk_device;
	std::vector<VkPhysicalDevice> _devices;
	std::map<int, VkPhysicalDevice> _suitable_devices;
};

