#pragma once
#include "Forwards.hpp"

#define FRAME_COUNT 2
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
		Renderer(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, int queueFamilyIndex, int width, int height);
		~Renderer();

		void Prepare();
		void Draw();

	private:
		void InitSwapchain();
		void InitRender();

		VkSurfaceKHR surface;
		VkPhysicalDevice physicalDevice;
		VkDevice device;
		int width;
		int height;
		int familyIndex;

		uint32_t queueFrameIndex = 0;

		uint32_t swapchainImageCount;
		VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
		VkExtent2D swapchainExtent {};
		VkSurfaceFormatKHR surfaceFormat {};
		VkSwapchainKHR swapchain {};

		VkCommandPool commandPool {};
		VkCommandBuffer commandBuffers[FRAME_COUNT];
		VkFence frameFences[FRAME_COUNT];
		VkSemaphore imageAvailableSemaphores[FRAME_COUNT];
		VkSemaphore renderFinishedSemaphores[FRAME_COUNT];
	};
}
