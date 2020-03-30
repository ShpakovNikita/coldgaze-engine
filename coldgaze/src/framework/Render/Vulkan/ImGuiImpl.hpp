#pragma once
#include "vulkan/vulkan_core.h"

namespace CG
{
	class Engine;

	namespace Vk
	{
		class Device;

		class ImGuiImpl
		{
		public:
			ImGuiImpl(const Engine& engine);
			~ImGuiImpl();

			void Init(float width, float height);
			void InitResources(VkRenderPass renderPass, VkQueue queue);

		private:
			const Engine& engine;
			const Vk::Device* device;

			VkImage fontImage = VK_NULL_HANDLE;
			VkDeviceMemory fontMemory = VK_NULL_HANDLE;
		};
	}
}
