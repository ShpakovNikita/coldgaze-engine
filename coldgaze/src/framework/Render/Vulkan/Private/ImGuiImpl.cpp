#include "Render/Vulkan/ImGuiImpl.hpp"
#include "core/engine.hpp"
#include "imgui/imgui.h"
#include "Render/Vulkan/Initializers.hpp"
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Debug.hpp"
#include "../Buffer.hpp"
#include "../Utils.hpp"

using namespace CG;

CG::Vk::ImGuiImpl::ImGuiImpl(const Engine& aEngine)
	: engine(aEngine)
	, device(aEngine.GetDevice())
{
	ImGui::CreateContext();
}

CG::Vk::ImGuiImpl::~ImGuiImpl()
{
	ImGui::DestroyContext();

	vkDestroyImage(device->logicalDevice, fontImage, nullptr);
	vkFreeMemory(device->logicalDevice, fontMemory, nullptr);
}

void CG::Vk::ImGuiImpl::Init(float width, float height)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(width, height);
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void CG::Vk::ImGuiImpl::InitResources([[ maybe_unused ]] VkRenderPass renderPass, [[ maybe_unused ]] VkQueue copyQueue)
{
	ImGuiIO& io = ImGui::GetIO();

	// Create font texture
	unsigned char* fontData;
	int texWidth, texHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
	[[ maybe_unused ]] VkDeviceSize uploadSize = static_cast<uint64_t>(texWidth) * static_cast<uint64_t>(texHeight) * 4 * sizeof(char);

	VkImageCreateInfo imageInfo = CG::Vk::Initializers::ImageCreateInfo();
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent.width = texWidth;
	imageInfo.extent.height = texHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageInfo, nullptr, &fontImage));
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device->logicalDevice, fontImage, &memReqs);
	VkMemoryAllocateInfo memAllocInfo = CG::Vk::Initializers::MemoryAllocateInfo();
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &fontMemory));
	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, fontImage, fontMemory, 0));

	// Image view
	VkImageViewCreateInfo viewInfo = CG::Vk::Initializers::ImageViewCreateInfo();
	viewInfo.image = fontImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;
	VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewInfo, nullptr, &fontView));

	Vk::Buffer stagingBuffer;

	VK_CHECK_RESULT(device->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		uploadSize, fontData));

	VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Prepare for transfer
	Utils::SetImageLayout(
		copyCmd,
		fontImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT);

	// Copy
	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = texWidth;
	bufferCopyRegion.imageExtent.height = texHeight;
	bufferCopyRegion.imageExtent.depth = 1;

	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer.buffer,
		fontImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion
	);

	// Prepare for shader read
	Utils::SetImageLayout(
		copyCmd,
		fontImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	device->FlushCommandBuffer(copyCmd, copyQueue, true);

	stagingBuffer.destroy();

	VkSamplerCreateInfo samplerInfo = Initializers::SamplerCreateInfo();
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &sampler));

	std::vector<VkDescriptorPoolSize> poolSizes = {
		Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = Initializers::DescriptorPoolCreateInfo(poolSizes, 2);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		Initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
	};
	VkDescriptorSetLayoutCreateInfo descriptorLayout = Initializers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkDescriptorSetAllocateInfo allocInfo = Initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));
	VkDescriptorImageInfo fontDescriptor = Initializers::DescriptorImageInfo(
		sampler,
		fontView,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		Initializers::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
	};
	vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = Initializers::PipelineCacheCreateInfo();
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(device->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

}

