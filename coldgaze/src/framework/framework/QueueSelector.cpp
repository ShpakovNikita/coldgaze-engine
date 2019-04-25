#include "QueueSelector.h"


QueueSelector::QueueSelector(VkPhysicalDevice a_device)
{
	reset_device(a_device);
}

QueueSelector::QueueSelector()
	: QueueSelector(VK_NULL_HANDLE)
{

}

QueueSelector::~QueueSelector()
{
}

void QueueSelector::reset_device(VkPhysicalDevice a_device)
{
	_device = a_device;
	if (_device != VK_NULL_HANDLE)
	{
		_indices = _find_queue_families(_device);
	}
}

const QueueFamilyIndices & QueueSelector::get_queue_family_indices()
{
	return _indices;
}

QueueFamilyIndices QueueSelector::_find_queue_families(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	int i = 0;
	for (const auto& queue_family : queue_families) {
		if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family = i;
		}

		if (indices.is_complete()) {
			break;
		}

		i++;
	}

	return indices;
}
