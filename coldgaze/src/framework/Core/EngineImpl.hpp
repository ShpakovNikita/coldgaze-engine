#pragma once
#include "engine.hpp"
#include <glm/glm.hpp>
#include "vulkan/vulkan_core.h"
#include "Render/Vulkan/Model.hpp"
#include "Render/Vulkan/Buffer.hpp"
#include <mutex>
#include <thread>

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
    class EngineImpl : public Engine
    {
    public:
        EngineImpl(CG::EngineConfig& engineConfig);
		virtual ~EngineImpl();

    protected:
        void RenderFrame(float deltaTime) override;
        void Prepare() override;
		void Cleanup() override;

		VkPhysicalDeviceFeatures GetEnabledDeviceFeatures() const override;

    private:
		struct UISettings {
			std::array<float, 50> frameTimes{};
			float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
			glm::vec4 bgColor = { 0.569f, 0.553f, 0.479f, 1.0f };
			bool isActive = true;
			bool drawWire = false;
		} uiData = {};

		struct ShaderUniformData
		{
			glm::mat4 projection;
			glm::mat4 view;
			glm::vec4 lightPos = glm::vec4(2.0f, 2.0f, -2.0f, 1.0f);
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
		void BuildCommandBuffers() override;

		void SetupDescriptors();
		void BindModelMaterials();

		void PrepareUniformBuffers();
		void UpdateUniformBuffers();

		void SetupSystems();

		void LoadModel(const std::string& modelFilePath);
		void LoadModelAsync(std::string modelFilePath);

		CG::Vk::UniformBufferVS* uniformBufferVS = nullptr;

		VkPipelineLayout pipelineLayout = {};
		VkDescriptorSetLayout descriptorSetLayout = {};
		VkDescriptorSet descriptorSet = {};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		std::unique_ptr<Vk::GLTFModel> testModel;
		std::mutex modelLoadingMutex;
		std::unique_ptr<std::thread> modelLoadingTread;
    };
}
