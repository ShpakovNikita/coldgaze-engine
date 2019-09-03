#include "DevicePicker.h"
#include <SDL2/SDL.h>
#include <signal.h>

DevicePicker::DevicePicker(const VScopedPtr<VkInstance> &instance)
	: _vk_device(VK_NULL_HANDLE)
{
	reset_instance(instance);
}


DevicePicker::~DevicePicker()
{
}

int DevicePicker::pick_best_device()
{
	for (const auto& device : _devices) {
		int score = get_device_rating(device);
		_suitable_devices[score] = device;
	}

	auto it = _suitable_devices.begin();
	_vk_device = it->second;

	if (_vk_device == VK_NULL_HANDLE || !is_device_suitable(it->first)) {
		CG_ASSERT(false);
		throw std::runtime_error("failed to find a suitable GPU!");
		return VK_ERROR_DEVICE_LOST;
	}

	return VK_SUCCESS;
}

int DevicePicker::reset_instance(const VScopedPtr<VkInstance> &instance)
{
	uint32_t devices_count = 0;
	vkEnumeratePhysicalDevices(instance, &devices_count, nullptr);
	
	if (devices_count == 0) {
		CG_ASSERT(false);
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
		return VK_ERROR_DEVICE_LOST;
	}

	_devices.resize(devices_count);
	vkEnumeratePhysicalDevices(instance, &devices_count, _devices.data());
	
	return VK_SUCCESS;
}

VkPhysicalDevice DevicePicker::get_device() const
{
	return _vk_device;
}

bool DevicePicker::is_device_suitable(int score)
{
	return score > 0;
}

int DevicePicker::get_device_rating(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(device, &device_properties);

	VkPhysicalDeviceFeatures device_features;
	vkGetPhysicalDeviceFeatures(device, &device_features);

	int score = 0;

	// Discrete GPUs have a significant performance advantage
	if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 1000;
	}

	// Maximum possible size of textures affects graphics quality
	score += device_properties.limits.maxImageDimension2D;

	// Application can't function without geometry shaders
	if (!device_features.geometryShader) {
		return 0;
	}

	return score;
}
