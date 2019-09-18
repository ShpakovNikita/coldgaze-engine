#include "Renderer.h"

using namespace CG;

namespace SRenderer
{
	static uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max)
	{
		return value < min ? min : (value > max ? max : value);
	}
}

enum {
	MAX_PRESENT_MODE_COUNT = 6, // At the moment in spec
	FRAME_COUNT = 2,
	PRESENT_MODE_MAILBOX_IMAGE_COUNT = 3,
	PRESENT_MODE_DEFAULT_IMAGE_COUNT = 2,
};

Renderer::Renderer(VkSurfaceKHR aSurface, VkPhysicalDevice aPhysicalDevice, VkDevice aDevice, int aWidth, int aHeight)
	: surface(aSurface)
	, physicalDevice(aPhysicalDevice)
	, device(aDevice)
	, width(aWidth)
	, height(aHeight)
{
}

Renderer::~Renderer()
{
}

void CG::Renderer::Prepare()
{
	InitSwapchain();
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

	if (vkCreateSwapchainKHR(device, &swapChainCreateInfo, 0, &swapchain) != VK_SUCCESS)
	{
		CG_ASSERT(false);
		throw std::runtime_error("Swapchain cannot be created!");
	}

	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);
}
