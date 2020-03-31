#pragma once

#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <cstring>

namespace CG
{
	namespace Vk
	{
		struct Buffer
		{
			VkDevice device;
			VkBuffer buffer = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkDescriptorBufferInfo descriptor;
			VkDeviceSize size = 0;
			VkDeviceSize alignment = 0;
			void* mapped = nullptr;

			VkBufferUsageFlags usageFlags;
			VkMemoryPropertyFlags memoryPropertyFlags;

			VkResult map(VkDeviceSize mappingSize = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
			{
				return vkMapMemory(device, memory, offset, mappingSize, 0, &mapped);
			}

			void unmap()
			{
				if (mapped)
				{
					vkUnmapMemory(device, memory);
					mapped = nullptr;
				}
			}

			VkResult bind(VkDeviceSize offset = 0)
			{
				return vkBindBufferMemory(device, buffer, memory, offset);
			}

			void setupDescriptor(VkDeviceSize aSize = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
			{
				descriptor.offset = offset;
				descriptor.buffer = buffer;
				descriptor.range = aSize;
			}

			void copyTo(void* data, VkDeviceSize aSize)
			{
				assert(mapped);
				memcpy(mapped, data, aSize);
			}

			VkResult flush(VkDeviceSize aSize = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
			{
				VkMappedMemoryRange mappedRange = {};
				mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				mappedRange.memory = memory;
				mappedRange.offset = offset;
				mappedRange.size = aSize;
				return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
			}

			/**
			* @note Only required for non-coherent memory
			*/
			VkResult invalidate(VkDeviceSize aSize = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
			{
				VkMappedMemoryRange mappedRange = {};
				mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
				mappedRange.memory = memory;
				mappedRange.offset = offset;
				mappedRange.size = aSize;
				return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
			}

			void destroy()
			{
				if (buffer)
				{
					vkDestroyBuffer(device, buffer, nullptr);
				}
				if (memory)
				{
					vkFreeMemory(device, memory, nullptr);
				}
			}
		};
	}
}