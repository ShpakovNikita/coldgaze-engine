#include "vulkan\vulkan_core.h"

struct SDL_Window;

namespace CG
{
    namespace Vk
    {
        class SwapChain
        {
        public:
            SwapChain(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
			
			void InitSurface(SDL_Window* window);
			void Create(uint32_t width, uint32_t height, bool vsync);

			uint32_t queueNodeIndex = UINT32_MAX;
			VkFormat colorFormat = {};
			VkColorSpaceKHR colorSpace = {};

        private:
            /**
            * Set instance, physical and logical device to use for the swapchain and get all required function pointers
            *
            * @param instance Vulkan instance to use
            * @param physicalDevice Physical device used to query properties and formats relevant to the swapchain
            * @param device Logical representation of the device to create the swapchain for
            *
            */
            void Connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

            VkInstance instance = {};
            VkDevice device = {};
            VkPhysicalDevice physicalDevice = {};
            VkSurfaceKHR surface = {};

            PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
            PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
            PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
            PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
            PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
            PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
            PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
            PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
            PFN_vkQueuePresentKHR fpQueuePresentKHR;
        };
    }
}
