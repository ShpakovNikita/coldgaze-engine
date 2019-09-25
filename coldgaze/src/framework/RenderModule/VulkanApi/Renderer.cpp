#include "Renderer.h"
#include "SystemCore/QueueSelector.h"

using namespace CG;

namespace SRenderer
{
	static uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max)
	{
		return value < min ? min : (value > max ? max : value);
	}
}

enum {
	MAX_PRESENT_MODE_COUNT = 6,
	PRESENT_MODE_MAILBOX_IMAGE_COUNT = 3,
	PRESENT_MODE_DEFAULT_IMAGE_COUNT = 2,
};

Renderer::Renderer(VkSurfaceKHR aSurface, VkPhysicalDevice aPhysicalDevice, VkDevice aDevice,
	int aQueueFamilyIndex, int aWidth, int aHeight)
	: surface(aSurface)
	, physicalDevice(aPhysicalDevice)
	, device(aDevice)
	, width(aWidth)
	, height(aHeight)
	, familyIndex(aQueueFamilyIndex)
{
}

Renderer::~Renderer()
{
	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void CG::Renderer::Prepare()
{
	InitSwapchain();
	InitRender();
}

void CG::Renderer::Draw()
{
	uint32_t index = (queueFrameIndex++) % FRAME_COUNT;
	vkWaitForFences(device, 1, &frameFences[index], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &frameFences[index]);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[index], VK_NULL_HANDLE, &imageIndex);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffers[index], &beginInfo);

	VkImageSubresourceRange subResourceRange = {};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subResourceRange.baseArrayLayer = 0;
	subResourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageMemoryBarrier clearMemoryBarrier = {};
	clearMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	clearMemoryBarrier.srcAccessMask = 0;
	clearMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	clearMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	clearMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	clearMemoryBarrier.srcQueueFamilyIndex = familyIndex;
	clearMemoryBarrier.dstQueueFamilyIndex = familyIndex;
	clearMemoryBarrier.image = swapchainImages[imageIndex];
	clearMemoryBarrier.subresourceRange = subResourceRange;

	// Change layout of image to be optimal for clearing
	vkCmdPipelineBarrier(commandBuffers[index],
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL, 1, &clearMemoryBarrier);
	vkCmdClearColorImage(commandBuffers[index],
		swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&VkClearColorValue({1.0f, 0, 0, 1.0f}), 1, &subResourceRange
	);

	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageMemoryBarrier.srcQueueFamilyIndex = familyIndex;
	imageMemoryBarrier.dstQueueFamilyIndex = familyIndex;
	imageMemoryBarrier.image = swapchainImages[imageIndex];
	imageMemoryBarrier.subresourceRange = subResourceRange;

	// Change layout of image to be optimal for presenting
	vkCmdPipelineBarrier(commandBuffers[index],
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
}

void CG::Renderer::InitSwapchain()
{
	//Use first available format
	uint32_t formatCount = 1;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0); // suppress validation layer
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, &surfaceFormat);
	surfaceFormat.format = surfaceFormat.format == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : surfaceFormat.format;

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);
	VkPresentModeKHR presentModes[MAX_PRESENT_MODE_COUNT];
	presentModeCount = presentModeCount > MAX_PRESENT_MODE_COUNT ? MAX_PRESENT_MODE_COUNT : presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;   // always supported.
	for (uint32_t i = 0; i < presentModeCount; ++i)
	{
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}
	swapchainImageCount = presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? PRESENT_MODE_MAILBOX_IMAGE_COUNT : PRESENT_MODE_DEFAULT_IMAGE_COUNT;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

	swapchainExtent = surfaceCapabilities.currentExtent;
	if (swapchainExtent.width == UINT32_MAX)
	{
		swapchainExtent.width = SRenderer::clamp_u32(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		swapchainExtent.height = SRenderer::clamp_u32(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.minImageCount = swapchainImageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = swapchainExtent;
	swapChainCreateInfo.imageArrayLayers = 1; // 2 for stereo
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.pQueueFamilyIndices = nullptr;

	if (vkCreateSwapchainKHR(device, &swapChainCreateInfo, 0, &swapchain) != VK_SUCCESS)
	{
		CG_ASSERT(false);
		throw std::runtime_error("Swapchain cannot be created!");
	}

	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);
}

void CG::Renderer::InitRender()
{
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = familyIndex;

	vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);

	VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
	commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocInfo.commandPool = commandPool;
	commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocInfo.commandBufferCount = FRAME_COUNT;

	vkAllocateCommandBuffers(device, &commandBufferAllocInfo, commandBuffers);

	VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[0]);
	vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[1]);
	vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[0]);
	vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[1]);

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	vkCreateFence(device, &fenceCreateInfo, nullptr, &frameFences[0]);
	vkCreateFence(device, &fenceCreateInfo, nullptr, &frameFences[1]);
}
