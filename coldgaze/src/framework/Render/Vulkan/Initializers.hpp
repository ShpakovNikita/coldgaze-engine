#pragma once

#include "vulkan/vulkan_core.h"

namespace CG
{
	namespace Vk
	{
		namespace Initializers
		{
			inline VkImageViewCreateInfo ImageViewCreateInfo()
			{
				VkImageViewCreateInfo imageViewCreateInfo{};
				imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				return imageViewCreateInfo;
			}

			inline VkImageCreateInfo ImageCreateInfo()
			{
				VkImageCreateInfo imageCreateInfo{};
				imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				return imageCreateInfo;
			}

			inline VkMemoryAllocateInfo MemoryAllocateInfo()
			{
				VkMemoryAllocateInfo memAllocInfo{};
				memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				return memAllocInfo;
			}
		}
	}
}