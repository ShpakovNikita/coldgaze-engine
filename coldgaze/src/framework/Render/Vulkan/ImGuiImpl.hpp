#pragma once
#include "vulkan/vulkan_core.h"
#include <memory>
#include "glm/ext/vector_float2.hpp"

namespace CG
{
	class Engine;

	namespace Vk
	{
		class Device;
		struct Buffer;

		class ImGuiImpl
		{
		public:
			struct PushConstBlock {
				glm::vec2 scale;
				glm::vec2 translate;
			} pushConstBlock;

			ImGuiImpl(Engine& engine);
			~ImGuiImpl();

			void Init(float width, float height);
			void InitResources(VkRenderPass renderPass, VkQueue queue);

		private:
			Engine& engine;
			const Vk::Device* device;

			VkImage fontImage = VK_NULL_HANDLE;
			VkDeviceMemory fontMemory = VK_NULL_HANDLE;
			VkImageView fontView = VK_NULL_HANDLE;
			VkSampler sampler;
			VkDescriptorPool descriptorPool;
			VkDescriptorSetLayout descriptorSetLayout;
			VkDescriptorSet descriptorSet;
			VkPipelineCache pipelineCache;
			VkPipelineLayout pipelineLayout;
			VkPipeline pipeline;

			std::unique_ptr<Buffer> vertexBuffer;
			std::unique_ptr<Buffer> indexBuffer;
		};
	}
}
