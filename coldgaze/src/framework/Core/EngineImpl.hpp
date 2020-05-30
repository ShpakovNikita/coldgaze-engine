#pragma once
#include "engine.hpp"
#include <glm/glm.hpp>
#include "vulkan/vulkan_core.h"
#include "Render/Vulkan/Model.hpp"
#include "Render/Vulkan/Buffer.hpp"
#include <mutex>
#include <thread>
#include <array>
#include "glm/ext/vector_float4.hpp"
#include "Render/Vulkan/Texture.hpp"

struct CameraComponent;

namespace CG 
{
	namespace Vk 
	{
		struct UniformBufferVS;
		class GLTFModel;
		class ImGuiImpl;
		class SkyBox;
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
		void CaptureEvent(const SDL_Event& event) override;

    private:
		struct UISettings {
			std::array<float, 50> frameTimes{};
			float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
			glm::vec4 bgColor = { 0.569f, 0.553f, 0.479f, 1.0f };
			bool isActive = true;
			bool drawWire = false;
			bool useSampleShading = false;
			float fov = 60.f;

		} uiData = {};

		struct ShaderUniformData
		{
			glm::mat4 projection;
			glm::mat4 view;
            std::array<glm::vec4, 6> lightPosPushConstants;
            std::array<glm::vec4, 6> lightColorPushConstants;
		} uboData = {};

		Vk::Buffer ubo;

		struct DescriptorSetLayouts {
			VkDescriptorSetLayout matrices;
			VkDescriptorSetLayout textures;
		} descriptorSetLayouts = {};

		struct RenderPipelines
		{
			VkPipeline wireframe;
			VkPipeline solidPBR_MSAA;
		} pipelines = {};

		struct Textures
		{
			Vk::Texture irradianceCube;
		} textures = {};

		void FlushCommandBuffer(VkCommandBuffer commandBuffer);

		void PreparePipelines();

		void DrawUI();

		void BuildUiCommandBuffers();
		void BuildCommandBuffers() override;

		void SetupDescriptors();
		void BindModelMaterials();
		void UnbindModelMaterials();

		void PrepareUniformBuffers();
		void UpdateUniformBuffers();

		void SetupSystems();

		void LoadModel(const std::string& modelFilePath);
		void LoadModelAsync(const std::string& modelFilePath);

		void LoadSkybox(const std::string& cubeMapFilePath);

		void GenerateIrradianceSampler();

		CG::Vk::UniformBufferVS* uniformBufferVS = nullptr;

		VkPipelineLayout pipelineLayout = {};
		VkDescriptorSetLayout descriptorSetLayout = {};
		VkDescriptorSet descriptorSet = {};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		std::unique_ptr<Vk::GLTFModel> testScene;
		std::unique_ptr<Vk::SkyBox> testSkybox;
    };
}
