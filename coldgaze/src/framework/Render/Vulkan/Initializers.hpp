#pragma once

#include "vulkan/vulkan_core.h"
#include <vector>

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

			inline VkBufferCreateInfo BufferCreateInfo(VkBufferUsageFlags usage, VkDeviceSize size)
			{
				VkBufferCreateInfo bufferCreateInfo{};
				bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferCreateInfo.usage = usage;
				bufferCreateInfo.size = size;
				return bufferCreateInfo;
			}
			
			inline VkCommandBufferBeginInfo CommandBufferBeginInfo()
			{
				VkCommandBufferBeginInfo commandBufferBeginInfo{};
				commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				return commandBufferBeginInfo;
			}

			inline VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool commandPool,
				VkCommandBufferLevel level, uint32_t bufferCount)
			{
				VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
				commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				commandBufferAllocateInfo.commandPool = commandPool;
				commandBufferAllocateInfo.level = level;
				commandBufferAllocateInfo.commandBufferCount = bufferCount;
				return commandBufferAllocateInfo;
			}

			inline VkImageMemoryBarrier ImageMemoryBarrier()
			{
				VkImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				return imageMemoryBarrier;
			}

			inline VkSubmitInfo SubmitInfo()
			{
				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				return submitInfo;
			}

			inline VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0)
			{
				VkFenceCreateInfo fenceCreateInfo{};
				fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
				fenceCreateInfo.flags = flags;
				return fenceCreateInfo;
			}

			inline VkSamplerCreateInfo SamplerCreateInfo()
			{
				VkSamplerCreateInfo samplerCreateInfo{};
				samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
				samplerCreateInfo.maxAnisotropy = 1.0f;
				return samplerCreateInfo;
			}

			inline VkDescriptorPoolSize DescriptorPoolSize(
				VkDescriptorType type,
				uint32_t descriptorCount)
			{
				VkDescriptorPoolSize descriptorPoolSize{};
				descriptorPoolSize.type = type;
				descriptorPoolSize.descriptorCount = descriptorCount;
				return descriptorPoolSize;
			}

			inline VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(
				const std::vector<VkDescriptorPoolSize>& poolSizes,
				uint32_t maxSets)
			{
				VkDescriptorPoolCreateInfo descriptorPoolInfo{};
				descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
				descriptorPoolInfo.pPoolSizes = poolSizes.data();
				descriptorPoolInfo.maxSets = maxSets;
				return descriptorPoolInfo;
			}

			inline VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(
				VkDescriptorType type,
				VkShaderStageFlags stageFlags,
				uint32_t binding,
				uint32_t descriptorCount = 1)
			{
				VkDescriptorSetLayoutBinding setLayoutBinding{};
				setLayoutBinding.descriptorType = type;
				setLayoutBinding.stageFlags = stageFlags;
				setLayoutBinding.binding = binding;
				setLayoutBinding.descriptorCount = descriptorCount;
				return setLayoutBinding;
			}

			inline VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
				const std::vector<VkDescriptorSetLayoutBinding>& bindings)
			{
				VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
				descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				descriptorSetLayoutCreateInfo.pBindings = bindings.data();
				descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
				return descriptorSetLayoutCreateInfo;
			}

			inline VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(
				VkDescriptorPool descriptorPool,
				const VkDescriptorSetLayout* pSetLayouts,
				uint32_t descriptorSetCount)
			{
				VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
				descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				descriptorSetAllocateInfo.descriptorPool = descriptorPool;
				descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
				descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
				return descriptorSetAllocateInfo;
			}

			inline VkDescriptorImageInfo DescriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
			{
				VkDescriptorImageInfo descriptorImageInfo{};
				descriptorImageInfo.sampler = sampler;
				descriptorImageInfo.imageView = imageView;
				descriptorImageInfo.imageLayout = imageLayout;
				return descriptorImageInfo;
			}

			inline VkWriteDescriptorSet WriteDescriptorSet(
				VkDescriptorSet dstSet,
				VkDescriptorType type,
				uint32_t binding,
				VkDescriptorBufferInfo* bufferInfo,
				uint32_t descriptorCount = 1)
			{
				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.dstSet = dstSet;
				writeDescriptorSet.descriptorType = type;
				writeDescriptorSet.dstBinding = binding;
				writeDescriptorSet.pBufferInfo = bufferInfo;
				writeDescriptorSet.descriptorCount = descriptorCount;
				return writeDescriptorSet;
			}

			inline VkWriteDescriptorSet WriteDescriptorSet(
				VkDescriptorSet dstSet,
				VkDescriptorType type,
				uint32_t binding,
				VkDescriptorImageInfo* imageInfo,
				uint32_t descriptorCount = 1)
			{
				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.dstSet = dstSet;
				writeDescriptorSet.descriptorType = type;
				writeDescriptorSet.dstBinding = binding;
				writeDescriptorSet.pImageInfo = imageInfo;
				writeDescriptorSet.descriptorCount = descriptorCount;
				return writeDescriptorSet;
			}

			inline VkPipelineCacheCreateInfo PipelineCacheCreateInfo()
			{
				VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
				pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
				return pipelineCacheCreateInfo;
			}
		}
	}
}