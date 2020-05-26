#pragma once

#include "vulkan/vulkan_core.h"
#include "Texture.hpp"


namespace CG
{
	namespace Vk
	{
		class Device;

		class Texture2D : 
			public Texture
		{
		public:
			void FromBuffer(
				const void* buffer,
				VkDeviceSize bufferSize,
				VkFormat format,
				uint32_t texWidth,
				uint32_t texHeight,
				Device* device,
				VkQueue copyQueue,
				VkFilter filter = VK_FILTER_LINEAR,
				VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
				VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		};
	}
}