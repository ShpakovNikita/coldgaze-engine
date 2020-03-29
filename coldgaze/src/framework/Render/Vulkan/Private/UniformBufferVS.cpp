#pragma once
#include "Render/Vulkan/UniformBufferVS.hpp"
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Debug.hpp"

using namespace CG::Vk;

void CG::Vk::UniformBufferVS::PrepareUniformBuffers(Device* vkDevice, VkDeviceSize size)
{
	VkDevice device = vkDevice->logicalDevice;

	VkMemoryRequirements memReqs;

	VkBufferCreateInfo bufferInfo = {};
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(size);
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));
	vkGetBufferMemoryRequirements(device, buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device, buffer, memory, 0));

	// Store information in the uniform's descriptor that is used by the descriptor set
	descriptor.buffer = buffer;
	descriptor.offset = 0;
	descriptor.range = sizeof(size);
}
