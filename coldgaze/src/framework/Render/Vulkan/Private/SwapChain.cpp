#include "Render/Vulkan/SwapChain.hpp"
#include <stdexcept>
#include "SDL2/SDL_vulkan.h"
#include <assert.h>
#include <vector>
#include "../Debug.hpp"

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetInstanceProcAddr(inst, "vk"#entrypoint)); \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		throw std::runtime_error("No instance procedure addres found!");                                                       \
	}                                                                   \
}

#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetDeviceProcAddr(dev, "vk"#entrypoint));   \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		throw std::runtime_error("No device procedure addres found!");                                                        \
	}                                                                   \
}

CG::Vk::SwapChain::SwapChain(VkInstance aInstance, VkPhysicalDevice aPhysicalDevice, VkDevice aDevice)
{
    Connect(aInstance, aPhysicalDevice, aDevice);
}

void CG::Vk::SwapChain::InitSurface(SDL_Window* window)
{
	if (!SDL_Vulkan_CreateSurface(window, instance, &surface))
	{
		throw std::runtime_error("Failed to create surface!");
	}

	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
	assert(queueCount >= 1);

	std::vector<VkQueueFamilyProperties> queueProps(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

	std::vector<VkBool32> supportsPresent(queueCount);
	for (uint32_t i = 0; i < queueCount; i++)
	{
		fpGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent[i]);
	}

	uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	uint32_t presentQueueNodeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queueCount; i++)
	{
		if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (supportsPresent[i] == VK_TRUE)
			{
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	assert(graphicsQueueNodeIndex != UINT32_MAX);
	assert(presentQueueNodeIndex != UINT32_MAX);

	queueNodeIndex = graphicsQueueNodeIndex;

	uint32_t formatCount;
	VK_CHECK_RESULT(fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL));
	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	VK_CHECK_RESULT(fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data()));

	// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
	// there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
	if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
	{
		colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
		colorSpace = surfaceFormats[0].colorSpace;
	}
	else
	{
		// iterate over the list of available surface format and
		// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
		bool found_B8G8R8A8_UNORM = false;
		for (auto&& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				colorFormat = surfaceFormat.format;
				colorSpace = surfaceFormat.colorSpace;
				found_B8G8R8A8_UNORM = true;
				break;
			}
		}

		// in case VK_FORMAT_B8G8R8A8_UNORM is not available
		// select the first available color format
		if (!found_B8G8R8A8_UNORM)
		{
			colorFormat = surfaceFormats[0].format;
			colorSpace = surfaceFormats[0].colorSpace;
		}
	}
}

void CG::Vk::SwapChain::Create(uint32_t width, uint32_t height, bool vsync)
{

}

void CG::Vk::SwapChain::Connect(VkInstance aInstance, VkPhysicalDevice aPhysicalDevice, VkDevice aDevice)
{
    instance = aInstance;
    physicalDevice = aPhysicalDevice;
    device = aDevice;
    GET_INSTANCE_PROC_ADDR(aInstance, GetPhysicalDeviceSurfaceSupportKHR);
    GET_INSTANCE_PROC_ADDR(aInstance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    GET_INSTANCE_PROC_ADDR(aInstance, GetPhysicalDeviceSurfaceFormatsKHR);
    GET_INSTANCE_PROC_ADDR(aInstance, GetPhysicalDeviceSurfacePresentModesKHR);
    GET_DEVICE_PROC_ADDR(aDevice, CreateSwapchainKHR);
    GET_DEVICE_PROC_ADDR(aDevice, DestroySwapchainKHR);
    GET_DEVICE_PROC_ADDR(aDevice, GetSwapchainImagesKHR);
    GET_DEVICE_PROC_ADDR(aDevice, AcquireNextImageKHR);
    GET_DEVICE_PROC_ADDR(aDevice, QueuePresentKHR);
}

#undef GET_INSTANCE_PROC_ADDR
#undef GET_DEVICE_PROC_ADDR