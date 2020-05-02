#pragma once
#include "vulkan/vulkan_core.h"
#include "Buffer.hpp"

namespace CG
{
	namespace Vk
	{
		class Device;

		struct UniformBufferVS
		{
			Buffer buffer = {};

			VkDescriptorBufferInfo descriptor;

			void PrepareUniformBuffers(Device* vkDevice, VkDeviceSize size);
		};
	}
}
