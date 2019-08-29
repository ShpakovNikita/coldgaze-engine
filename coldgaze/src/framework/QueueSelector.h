#pragma once
#include "Forwards.hpp"

struct QueueFamilyIndices {
	int graphics_family = -1;

	bool is_complete() {
		return graphics_family >= 0;
	}
};

class QueueSelector
{
public:
	QueueSelector();
	QueueSelector(VkPhysicalDevice a_device);
	~QueueSelector();

	void reset_device(VkPhysicalDevice a_device);
	const QueueFamilyIndices &get_queue_family_indices();

private:
	static QueueFamilyIndices _find_queue_families(VkPhysicalDevice device);
	QueueFamilyIndices _indices;
	VkPhysicalDevice _device;
};

