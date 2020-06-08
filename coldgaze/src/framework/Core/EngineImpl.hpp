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

		VkPhysicalDeviceFeatures2 GetEnabledDeviceFeatures() const override;
		void CaptureEvent(const SDL_Event& event) override;

        void OnWindowResize() override;

    private:
        struct AccelerationStructure {
            VkDeviceMemory memory;
            VkAccelerationStructureNV accelerationStructure;
            glm::mat4 transform;
            uint64_t handle;
        };

        AccelerationStructure topLevelAS;

        std::vector<AccelerationStructure> blasData;

        struct GeometryInstance {
            glm::mat3x4 transform;
            uint32_t instanceId : 24;
            uint32_t mask : 8;
            uint32_t instanceOffset : 24;
            uint32_t flags : 8;
            uint64_t accelerationStructureHandle;
        };

		struct UISettings {
			std::array<float, 50> frameTimes{};
			float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
			glm::vec4 bgColor = { 0.569f, 0.553f, 0.479f, 1.0f };
			bool isActive = true;
			bool drawWire = false;
			bool useSampleShading = false;
			float fov = 60.f;

		} uiData = {};

        struct SceneShaderUniformData
        {
            glm::mat4 projection;
            glm::mat4 model;
            glm::mat4 view;
			glm::vec4 cameraPos;

            glm::mat4 invProjection;
            glm::mat4 invView;

            glm::vec4 globalLightDir;
            glm::vec4 globalLightColor;
        } sceneUboData = {};

		Vk::Buffer sceneUbo;

        struct DescriptorSetLayout
        {
            bool created = false;
            VkDescriptorSetLayout layout;
        };

		struct DescriptorSetLayouts {
            DescriptorSetLayout rtxRaygenLayout;
            DescriptorSetLayout rtxRayhitLayout;
            DescriptorSetLayout rtxRaymissLayout;
		} descriptorSetLayouts = {};

        struct DescriptorSets {
            VkDescriptorSet scene;
            VkDescriptorSet rtxRaygen;
            VkDescriptorSet rtxRayhit;
            VkDescriptorSet rtxRaymiss;
        } descriptorSets = {};

		struct RenderPipelines
		{
			VkPipeline RTX;
		} pipelines = {};

		void FlushCommandBuffer(VkCommandBuffer commandBuffer);

		void DrawUI();

		void BuildUiCommandBuffers();
		void BuildCommandBuffers() override;

		void SetupDescriptorsPool();
		void BindModelMaterials();
		void UnbindModelMaterials();

		void PrepareUniformBuffers();
		void UpdateUniformBuffers();

		void SetupSystems();

		void LoadModel(const std::string& modelFilePath);
		void LoadModelAsync(const std::string& modelFilePath);

		void LoadSkybox(const std::string& cubeMapFilePath);

		void CreateBottomLevelAccelerationStructure(const VkGeometryNV* geometries, uint32_t blasIndex, size_t geomCount);
		void CreateTopLevelAccelerationStructure();

		void LoadNVRayTracingProcs();

		void CreateNVRayTracingGeometry();
		void DestroyNVRayTracingGeometry();

		void CreateNVRayTracingStoreImage();
		void DestroyNVRayTracingStoreImage();

        void CreateShaderBindingTable();
        VkDeviceSize CopyShaderIdentifier(uint8_t* data, const uint8_t* shaderHandleStorage, uint32_t groupIndex);

        void CreateRTXPipeline();
        void DestroyRTXPipeline();

        void CreateRTXPipelineLayout();
        void DestroyRTXPipelineLayout();

        void SetupRTXModelDescriptorSets();
        void SetupRTXEnviromentDescriptorSet();
        void DrawRayTracingData(uint32_t swapChainImageIndex);

		CG::Vk::UniformBufferVS* uniformBufferVS = nullptr;

        struct
        {
            VkPipelineLayout rtxPipelineLayout = {};
        } pipelineLayouts = {};

		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

		Vk::Texture2D emptyTexture;
		Vk::Texture2D cubemapTexture;

		std::unique_ptr<Vk::GLTFModel> testScene;

        Vk::Buffer shaderBindingTable;

        PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
        PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
        PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
        PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
        PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
        PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
        PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
        PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
        PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;

        struct StorageImage {
            VkDeviceMemory memory;
            VkImage image;
            VkImageView view;
            VkFormat format;
        } storageImage;

        // TODO: remove
        Vk::Buffer vertexBuffer;
        Vk::Buffer indexBuffer;
    };
}
