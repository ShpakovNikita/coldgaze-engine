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

			inline VkPushConstantRange PushConstantRange(
				VkShaderStageFlags stageFlags,
				uint32_t size,
				uint32_t offset)
			{
				VkPushConstantRange pushConstantRange{};
				pushConstantRange.stageFlags = stageFlags;
				pushConstantRange.offset = offset;
				pushConstantRange.size = size;
				return pushConstantRange;
			}

			inline VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(
				const VkDescriptorSetLayout* pSetLayouts,
				uint32_t setLayoutCount = 1)
			{
				VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
				pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
				pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
				pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
				return pipelineLayoutCreateInfo;
			}

			inline VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(
				VkPrimitiveTopology topology,
				VkPipelineInputAssemblyStateCreateFlags flags,
				VkBool32 primitiveRestartEnable)
			{
				VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
				pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
				pipelineInputAssemblyStateCreateInfo.topology = topology;
				pipelineInputAssemblyStateCreateInfo.flags = flags;
				pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
				return pipelineInputAssemblyStateCreateInfo;
			}

			inline VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(
				VkPolygonMode polygonMode,
				VkCullModeFlags cullMode,
				VkFrontFace frontFace,
				VkPipelineRasterizationStateCreateFlags flags = 0)
			{
				VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
				pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
				pipelineRasterizationStateCreateInfo.cullMode = cullMode;
				pipelineRasterizationStateCreateInfo.frontFace = frontFace;
				pipelineRasterizationStateCreateInfo.flags = flags;
				pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
				pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
				return pipelineRasterizationStateCreateInfo;
			}

			inline VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(
				uint32_t attachmentCount,
				const VkPipelineColorBlendAttachmentState* pAttachments)
			{
				VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
				pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
				pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
				pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
				return pipelineColorBlendStateCreateInfo;
			}

			inline VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(
				VkBool32 depthTestEnable,
				VkBool32 depthWriteEnable,
				VkCompareOp depthCompareOp)
			{
				VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
				pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
				pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
				pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
				pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
				pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;
				pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
				return pipelineDepthStencilStateCreateInfo;
			}

			inline VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(
				uint32_t viewportCount,
				uint32_t scissorCount,
				VkPipelineViewportStateCreateFlags flags = 0)
			{
				VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
				pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
				pipelineViewportStateCreateInfo.viewportCount = viewportCount;
				pipelineViewportStateCreateInfo.scissorCount = scissorCount;
				pipelineViewportStateCreateInfo.flags = flags;
				return pipelineViewportStateCreateInfo;
			}

			inline VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo(
				VkSampleCountFlagBits rasterizationSamples,
				VkPipelineMultisampleStateCreateFlags flags = 0)
			{
				VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
				pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
				pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
				pipelineMultisampleStateCreateInfo.flags = flags;
				return pipelineMultisampleStateCreateInfo;
			}

			inline VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(
				const std::vector<VkDynamicState>& pDynamicStates,
				VkPipelineDynamicStateCreateFlags flags = 0)
			{
				VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
				pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
				pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
				pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pDynamicStates.size());
				pipelineDynamicStateCreateInfo.flags = flags;
				return pipelineDynamicStateCreateInfo;
			}

			inline VkGraphicsPipelineCreateInfo PipelineCreateInfo(
				VkPipelineLayout layout,
				VkRenderPass renderPass,
				VkPipelineCreateFlags flags = 0)
			{
				VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
				pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
				pipelineCreateInfo.layout = layout;
				pipelineCreateInfo.renderPass = renderPass;
				pipelineCreateInfo.flags = flags;
				pipelineCreateInfo.basePipelineIndex = -1;
				pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
				return pipelineCreateInfo;
			}

			inline VkVertexInputBindingDescription VertexInputBindingDescription(
				uint32_t binding,
				uint32_t stride,
				VkVertexInputRate inputRate)
			{
				VkVertexInputBindingDescription vInputBindDescription{};
				vInputBindDescription.binding = binding;
				vInputBindDescription.stride = stride;
				vInputBindDescription.inputRate = inputRate;
				return vInputBindDescription;
			}

			inline VkVertexInputAttributeDescription VertexInputAttributeDescription(
				uint32_t binding,
				uint32_t location,
				VkFormat format,
				uint32_t offset)
			{
				VkVertexInputAttributeDescription vInputAttribDescription{};
				vInputAttribDescription.location = location;
				vInputAttribDescription.binding = binding;
				vInputAttribDescription.format = format;
				vInputAttribDescription.offset = offset;
				return vInputAttribDescription;
			}

			inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo()
			{
				VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
				pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
				return pipelineVertexInputStateCreateInfo;
			}

			inline VkViewport Viewport(
				float width,
				float height,
				float minDepth,
				float maxDepth)
			{
				VkViewport viewport = {};
				viewport.width = width;
				viewport.height = height;
				viewport.minDepth = minDepth;
				viewport.maxDepth = maxDepth;
				return viewport;
			}

			inline VkPhysicalDeviceRayTracingPropertiesNV PhysicalDeviceRayTracingPropertiesNV()
			{
				VkPhysicalDeviceRayTracingPropertiesNV rtProperties = {};
				rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
				return rtProperties;
			}
		}
	}
}