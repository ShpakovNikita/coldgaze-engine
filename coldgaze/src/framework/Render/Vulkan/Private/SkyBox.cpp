#include "Render/Vulkan/SkyBox.hpp"
#include "stb_image.h"
#include "Render/Vulkan/Exceptions.hpp"
#include "Render/Vulkan/Initializers.hpp"
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Debug.hpp"
#include "Render/Vulkan/Utils.hpp"
#include "Render/Vulkan/Texture2D.hpp"
#include <array>
#include <vector>
#include "Core/Engine.hpp"
#include "ECS/Components/CameraComponent.hpp"


CG::Vk::SkyBox::SkyBox(Engine& aEngine)
    : engine(aEngine)
{}

void CG::Vk::SkyBox::LoadFromFile(const std::string& fileName, Device* device, VkQueue copyQueue)
{
    vkDevice = device;
    queue = copyQueue;

    stbi_set_flip_vertically_on_load(true);

    int imageWidth, imageHeight, nrComponents;
    float* data = stbi_loadf(fileName.c_str(), &imageWidth, &imageHeight, &nrComponents, 0);

    stbi_set_flip_vertically_on_load(false);

    if (data)
    {
        sphericalSkyboxTexture = std::make_unique<Texture2D>();

        uint32_t width = static_cast<uint32_t>(imageWidth);
        uint32_t height = static_cast<uint32_t>(imageHeight);

        uint32_t imageSize = width * height * 3 * sizeof(float);

        TextureSampler sampler;
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sphericalSkyboxTexture->FromBuffer(data, imageSize, VK_FORMAT_R32G32B32_SFLOAT, width, height, device,
            copyQueue, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_TILING_LINEAR, sampler);

        stbi_image_free(data);

        CreateBoxModel();
        PrepareUniformBuffers();
    }
    else
    {
        throw AssetLoadingException("Failed to load hdr environment map! Make sure that it has RGBE or RGB format!");
    }
}

void CG::Vk::SkyBox::Draw(VkCommandBuffer commandBuffer)
{
    VkDeviceSize offsets[1] = { 0 };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.matrixDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &descriptorSets.textureDescriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

void CG::Vk::SkyBox::PreparePipeline(VkRenderPass renderPass, VkPipelineCache pipelineCache)
{
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        Vk::Initializers::PipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            0,
            VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        Vk::Initializers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_FRONT_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            0);

    VkPipelineColorBlendAttachmentState blendAttachmentState =
        Vk::Initializers::PipelineColorBlendAttachmentState(
            0xf,
            VK_FALSE);

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        Vk::Initializers::PipelineColorBlendStateCreateInfo(
            1,
            &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        Vk::Initializers::PipelineDepthStencilStateCreateInfo(
            VK_FALSE,
            VK_FALSE,
            VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState =
        Vk::Initializers::PipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        Vk::Initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT,
            0);

    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
        Vk::Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables, 0);

    // Vertex bindings and attributes
    VkVertexInputBindingDescription vertexInputBinding =
        Vk::Initializers::VertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);

    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
        Vk::Initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 0: Position
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState = Vk::Initializers::PipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vk::Initializers::PipelineCreateInfo(pipelineLayout, renderPass, 0);
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputState;

    multisampleState.rasterizationSamples = static_cast<VkSampleCountFlagBits>(engine.GetSampleCount());
    multisampleState.sampleShadingEnable = VK_TRUE;
    multisampleState.minSampleShading = 0.25f;

    // Skybox pipeline (background cube)
    shaderStages[0] = engine.LoadShader(engine.GetAssetPath() + "shaders/compiled/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = engine.LoadShader(engine.GetAssetPath() + "shaders/compiled/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(vkDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
}

void CG::Vk::SkyBox::UpdateCameraUniformBuffer(const CameraComponent* cameraComponent)
{
    uboData.projection = cameraComponent->uboVS.projectionMatrix;
    uboData.view = cameraComponent->uboVS.viewMatrix;

    ubo.CopyTo(&uboData, sizeof(uboData));
}

void CG::Vk::SkyBox::SetupDescriptorSet(VkDescriptorPool descriptorPool)
{
    const std::vector<VkDescriptorPoolSize> poolSizes =
    {
        Vk::Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        Vk::Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo = Vk::Initializers::DescriptorPoolCreateInfo(poolSizes, static_cast<uint32_t>(poolSizes.size()));
    VK_CHECK_RESULT(vkCreateDescriptorPool(vkDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));



    const std::vector<VkDescriptorSetLayoutBinding> matricesSetLayoutBindings =
    {
        Vk::Initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo matricesDescriptorSetLayoutCreateInfo = Vk::Initializers::DescriptorSetLayoutCreateInfo(matricesSetLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &matricesDescriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.matrices));

    const std::vector<VkDescriptorSetLayoutBinding> texturesSetLayoutBindings =
    {
        Vk::Initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo texturesDescriptorSetLayoutCreateInfo = Vk::Initializers::DescriptorSetLayoutCreateInfo(texturesSetLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &texturesDescriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.textures));

    const std::vector<VkDescriptorSetLayout> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Vk::Initializers::PipelineLayoutCreateInfo(setLayouts);

    VK_CHECK_RESULT(vkCreatePipelineLayout(vkDevice->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));



    const VkDescriptorSetAllocateInfo matricesAllocInfo = Vk::Initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &matricesAllocInfo, &descriptorSets.matrixDescriptorSet));
    VkWriteDescriptorSet matricesWriteDescriptorSet = Vk::Initializers::WriteDescriptorSet(descriptorSets.matrixDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo.descriptor);
    vkUpdateDescriptorSets(vkDevice->logicalDevice, 1, &matricesWriteDescriptorSet, 0, nullptr);

    const VkDescriptorSetAllocateInfo texturesAllocInfo = Vk::Initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &texturesAllocInfo, &descriptorSets.textureDescriptorSet));
    std::vector<VkWriteDescriptorSet> texturesWriteDescriptorSets =
    {
        Vk::Initializers::WriteDescriptorSet(descriptorSets.textureDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &sphericalSkyboxTexture->descriptor),
    };

    vkUpdateDescriptorSets(vkDevice->logicalDevice, static_cast<uint32_t>(texturesWriteDescriptorSets.size()), texturesWriteDescriptorSets.data(), 0, nullptr);

}

CG::Vk::Texture2D* CG::Vk::SkyBox::GetTexture2D() const
{
    return sphericalSkyboxTexture.get();
}

void CG::Vk::SkyBox::CreateBoxModel()
{
    std::array<float, 6 * 3 * 6> vertexBuffer = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    for (size_t i = 0; i < vertexBuffer.size(); ++i)
    {
        // This step is needed for wide camera FOVs
        vertexBuffer[i] *= 40.0f;
    }

    size_t vertexBufferSize = vertexBuffer.size() * sizeof(float);
    vertexCount = static_cast<uint32_t>(vertexBufferSize / sizeof(Vertex));

    // We are creating this buffers to copy them on local memory for better performance
    Buffer vertexStaging;

    VK_CHECK_RESULT(vkDevice->CreateBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &vertexStaging,
        vertexBufferSize,
        vertexBuffer.data()));

    VK_CHECK_RESULT(vkDevice->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vertices,
        vertexBufferSize));

    VkCommandBuffer copyCmd = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(
        copyCmd,
        vertexStaging.buffer,
        vertices.buffer,
        1,
        &copyRegion);

    vkDevice->FlushCommandBuffer(copyCmd, queue, true);

    vertexStaging.Destroy();
}

void CG::Vk::SkyBox::PrepareUniformBuffers()
{
    VK_CHECK_RESULT(vkDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &ubo,
        sizeof(uboData)));

    // Map persistent
    VK_CHECK_RESULT(ubo.Map());
}
