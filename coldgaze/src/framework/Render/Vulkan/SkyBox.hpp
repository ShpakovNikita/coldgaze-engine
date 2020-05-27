#pragma once

#include <string>
#include <memory>

#include "vulkan\vulkan_core.h"
#include "Buffer.hpp"
#include <glm/glm.hpp>
#include "glm\ext\vector_float3.hpp"

struct CameraComponent;

namespace CG
{
    class Engine;

    namespace Vk
    {
        class Device;
        class Texture2D;

        class SkyBox
        {
        public:
            SkyBox(Engine& engine);

            void LoadFromFile(const std::string& fileName, Device* device, VkQueue copyQueue);
            void Draw(VkCommandBuffer commandBuffer);

            void PreparePipeline(VkRenderPass renderPass, VkPipelineCache pipelineCache);
            void UpdateCameraUniformBuffer(const CameraComponent* cameraComponent);

            void SetupDescriptorSet(VkDescriptorPool descriptorPool);

        private:
            struct Vertex
            {
                glm::vec3 pos;
            };

            struct ShaderUniformData
            {
                glm::mat4 projection;
                glm::mat4 view;
            } uboData = {};

            struct DescriptorSetLayouts {
                VkDescriptorSetLayout matrices;
                VkDescriptorSetLayout textures;
            } descriptorSetLayouts = {};

            struct DescriptorSets {
                VkDescriptorSet matrixDescriptorSet;
                VkDescriptorSet textureDescriptorSet;
            } descriptorSets = {};

            void CreateBoxModel();
            void PrepareUniformBuffers();

            Device* vkDevice = nullptr;
            VkQueue queue;
            VkPipeline pipeline;
            VkPipelineLayout pipelineLayout;

            Buffer vertices;
            Buffer ubo;
            uint32_t vertexCount;
            Engine& engine;

            std::unique_ptr<Texture2D> sphericalSkyboxTexture;
        };
    }
}
