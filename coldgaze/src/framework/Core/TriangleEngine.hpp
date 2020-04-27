#pragma once
#include "engine.hpp"
#include <glm/glm.hpp>
#include "vulkan/vulkan_core.h"

namespace CG { namespace Vk { class ImGuiImpl; } }

namespace CG 
{
	namespace Vk 
	{
		struct UniformBufferVS;
	}
	struct EngineConfig; 
}

namespace CG
{
    class TriangleEngine : public Engine
    {
    public:
        TriangleEngine(CG::EngineConfig& engineConfig);
		virtual ~TriangleEngine();

    protected:
        void RenderFrame(float deltaTime) override;
        void Prepare() override;
		void Cleanup() override;

    private:
		struct UISettings {
			std::array<float, 50> frameTimes{};
			float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
		} uiSettings;

		VkCommandBuffer GetReadyCommandBuffer(); 
		void FlushCommandBuffer(VkCommandBuffer commandBuffer);

        void PrepareVertices();
        void SetupDescriptorSetLayout();
		void PreparePipelines();
		void SetupDescriptorPool();
		void BuildCommandBuffers();

		void RenderScene();

		void SetupCamera();
		void PrepareImgui();

		void DrawUI();
		void BuildUiCommandBuffers();

		// Vertex buffer and attributes
		struct 
		{
			VkDeviceMemory memory;	 // Handle to the device memory for this buffer
			VkBuffer buffer;		 // Handle to the Vulkan buffer object that the memory is bound to
		} vertices = {};

		// Index buffer
		struct 
		{
			VkDeviceMemory memory;
			VkBuffer buffer;
			uint32_t count;
		} indices = {};

		CG::Vk::UniformBufferVS* uniformBufferVS = nullptr;

		VkPipelineLayout pipelineLayout = {};
		VkDescriptorSetLayout descriptorSetLayout = {};
		VkPipeline pipeline = {};
		VkDescriptorSet descriptorSet = {};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		std::unique_ptr<Vk::ImGuiImpl> imGui;
    };
}
