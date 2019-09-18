#pragma once
#include "Forwards.hpp"

#define MAX_SWAPCHAIN_IMAGES 3

enum class eRenderApi
{
	none = 0,
	vulkan, 
	size,
};

namespace CG
{
	class Renderer
	{
	public:
		Renderer(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, int width, int height);
		~Renderer();

		void Prepare();

	private:
		void InitSwapchain();

		VkSurfaceKHR surface;
		VkPhysicalDevice physicalDevice;
		VkDevice device;
		int width;
		int height;

		uint32_t swapchainImageCount;
		VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
		VkExtent2D swapchainExtent {};
		VkSurfaceFormatKHR surfaceFormat {};
		VkSwapchainKHR swapchain {};
	};
}
