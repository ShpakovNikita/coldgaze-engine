#pragma once
#include "engine.hpp"
#include <glm/glm.hpp>
#include "vulkan/vulkan_core.h"
#include "Render/Vulkan/Model.hpp"
#include "Render/Vulkan/Buffer.hpp"

struct CameraComponent;

namespace CG 
{
	namespace Vk 
	{
		struct UniformBufferVS;
		class GLTFModel;
		class ImGuiImpl;
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
			glm::vec4 bgColor = { 0.0f, 0.0f, 0.2f, 1.0f };
			bool isActive = true;
		} uiData = {};

		struct ShaderUniformData
		{
			glm::mat4 projection;
			glm::mat4 view;
			glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);
		} uboData = {};

		Vk::Buffer ubo;

		struct DescriptorSetLayouts {
			VkDescriptorSetLayout matrices;
			VkDescriptorSetLayout textures;
		} descriptorSetLayouts = {};

		struct RenderPipelines
		{
			VkPipeline solid;
			VkPipeline wireframe;
		} pipelines = {};

		void FlushCommandBuffer(VkCommandBuffer commandBuffer);

		void PreparePipelines();

		void DrawUI();

		void BuildUiCommandBuffers();
		void BuildCommandBuffers();

		void SetupDescriptors();

		void PrepareUniformBuffers();
		void UpdateUniformBuffers();

		void SetupSystems();

		void LoadModel();

		CG::Vk::UniformBufferVS* uniformBufferVS = nullptr;

		VkPipelineLayout pipelineLayout = {};
		VkDescriptorSetLayout descriptorSetLayout = {};
		VkPipeline pipeline = {};
		VkDescriptorSet descriptorSet = {};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		std::unique_ptr<Vk::GLTFModel> testModel;

		CameraComponent* camComp = nullptr;
    };
}
