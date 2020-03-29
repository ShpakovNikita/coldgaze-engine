#pragma once
#include "vulkan/vulkan_core.h"

namespace CG
{
	namespace Vk
	{
		class Device;

		struct UniformBufferVS
		{
			VkDeviceMemory memory;
			VkBuffer buffer;
			VkDescriptorBufferInfo descriptor;

			void PrepareUniformBuffers(Device* vkDevice, VkDeviceSize size);
		};
	}
}
