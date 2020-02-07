#include "Render/Vulkan/SwapChain.hpp"
#include <stdexcept>

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