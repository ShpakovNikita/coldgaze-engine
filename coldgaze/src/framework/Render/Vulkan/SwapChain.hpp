#include "vulkan\vulkan_core.h"
#include <vector>

struct SDL_Window;

namespace CG
{
    namespace Vk
    {
        class SwapChain
        {
        public:
			struct SwapChainBuffer {
				VkImage image;
				VkImageView view;
			};

			/** @brief Creates swapchain instance and binds all needed Vulkan procedure addresses  */
            SwapChain(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
			
			/** @brief Creates the platform specific surface abstraction of the native platform window used for presentation */
			void InitSurface(SDL_Window* window);

			/**
			* Create the swapchain and get it's images with given width and height
			*
			* @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
			* @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
			* @param vsync (Optional) Can be used to force vsync'd rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
			*/
			void Create(uint32_t* width, uint32_t* height, bool vsync);

			/**
			* Queue an image for presentation
			*
			* @param queue Presentation queue for presenting the image
			* @param imageIndex Index of the swapchain image to queue for presentation
			* @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
			*
			* @return VkResult of the queue presentation
			*/
			VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

			/**
			* Acquires the next image in the swap chain
			*
			* @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
			* @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
			*
			* @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
			*
			* @return VkResult of the image acquisition
			*/
			VkResult AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);

			uint32_t queueNodeIndex = UINT32_MAX;
			VkFormat colorFormat = {};
			VkColorSpaceKHR colorSpace = {};
			VkSwapchainKHR swapChain = VK_NULL_HANDLE;
			uint32_t imageCount = UINT32_MAX;
			std::vector<SwapChainBuffer> buffers;
			std::vector<VkImage> images;

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

			PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR = {};
            PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR = {};
            PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR = {};
            PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR = {};
            PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR = {};
            PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR = {};
            PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR = {};
            PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR = {};
            PFN_vkQueuePresentKHR fpQueuePresentKHR = {};
        };
    }
}
