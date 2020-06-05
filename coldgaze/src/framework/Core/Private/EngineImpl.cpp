#include "Core\EngineImpl.hpp"
#include "Render\Vulkan\Debug.hpp"
#include "Render\Vulkan\Device.hpp"
#include "Core\EngineConfig.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stddef.h>
#include "Render\Vulkan\SwapChain.hpp"
#include "entt\entity\registry.hpp"
#include "ECS\Components\CameraComponent.hpp"
#include "ECS\Systems\CameraSystem.hpp"
#include <memory>
#include "Render\Vulkan\ImGuiImpl.hpp"
#include "imgui\imgui.h"
#include "ECS\Systems\LightSystem.hpp"
#include "Render\Vulkan\Model.hpp"
#include "Render\Vulkan\Initializers.hpp"
#include "Render\Vulkan\Texture2D.hpp"
#include <include/nfd.h>
#include "SDL2\SDL_messagebox.h"
#include "Render\Vulkan\Exceptions.hpp"
#include "Render\Vulkan\SkyBox.hpp"
#include "SDL2\SDL_events.h"
#include "Render\Vulkan\Texture.hpp"
#include "Render\Vulkan\Utils.hpp"
#include "Render\Vulkan\LayoutDescriptor.hpp"
#include <algorithm>

using namespace CG;

constexpr uint32_t kIndexRaygen = 0;
constexpr uint32_t kIndexMiss = 1;
constexpr uint32_t kIndexClosestHit = 2;

CG::EngineImpl::EngineImpl(CG::EngineConfig& engineConfig)
    : CG::Engine(engineConfig)
{
    enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    enabledDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    enabledDeviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
}

CG::EngineImpl::~EngineImpl() = default;

void CG::EngineImpl::RenderFrame(float deltaTime)
{
	UpdateUniformBuffers();

	PrepareFrame();

	imGui->UpdateUI(deltaTime);

	BuildCommandBuffers();

	// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
	VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo localSubmitInfo = {};
	localSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	localSubmitInfo.pWaitDstStageMask = &waitStageMask;
	localSubmitInfo.pWaitSemaphores = &semaphores.presentComplete;
	localSubmitInfo.waitSemaphoreCount = 1;
	localSubmitInfo.pSignalSemaphores = &semaphores.renderComplete;
	localSubmitInfo.signalSemaphoreCount = 1;
	localSubmitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
	localSubmitInfo.commandBufferCount = 1;

	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &localSubmitInfo, VK_NULL_HANDLE));

	SubmitFrame();
}

void CG::EngineImpl::Prepare()
{
    Engine::Prepare();

    InitRayTracing();
    LoadNVRayTracingProcs();

	SetupSystems();

    PrepareUniformBuffers();

    emptyTexture.LoadFromFile(GetAssetPath() + "textures/FFFFFF-1.png", vkDevice, queue);

    LoadSkybox(GetAssetPath() + "textures/hdr/Malibu_Overlook_3k.hdr");
    LoadModelAsync(GetAssetPath() + "models/FlightHelmet/glTF/FlightHelmet.gltf");

	PreparePipelines();


    // CreateNVRayTracingGeometry();
    CreateNVRayTracingStoreImage();
    CreateRTXPipeline();
    CreateShaderBindingTable();
    CreateRTXDescriptorSets();


	BuildCommandBuffers();

    UpdateUniformBuffers();
}

void CG::EngineImpl::Cleanup()
{
    emptyTexture.Destroy();

	testSkybox = nullptr;
	testScene = nullptr;
	imGui = nullptr;

    textures.irradianceCube.Destroy();

	Engine::Cleanup();
}

VkPhysicalDeviceFeatures CG::EngineImpl::GetEnabledDeviceFeatures() const
{
	VkPhysicalDeviceFeatures availableFeatures = vkDevice->features;
	VkPhysicalDeviceFeatures enabledFeatures = {};

	if (availableFeatures.fillModeNonSolid)
	{
		enabledFeatures.fillModeNonSolid = VK_TRUE;
	}


    if (availableFeatures.sampleRateShading) {
        enabledFeatures.sampleRateShading = VK_TRUE;
    }

	return enabledFeatures;
}

void CG::EngineImpl::CaptureEvent(const SDL_Event& event)
{
	switch (event.type)
	{
		case SDL_KEYDOWN:
		{
			if (event.key.keysym.sym == SDLK_f) {
				cameraComponent->position = glm::vec3(0.0f, 0.0f, -cameraComponent->zoom);
				cameraComponent->rotation = glm::vec3(0.0f, 0.0f, 0.0f);
			}
		}
		break;
	}
}

void CG::EngineImpl::FlushCommandBuffer(VkCommandBuffer commandBuffer)
{
	assert(commandBuffer != VK_NULL_HANDLE); 

	vkDevice->FlushCommandBuffer(commandBuffer, queue, true);
}

void CG::EngineImpl::PreparePipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = Vk::Initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = Vk::Initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCreateInfo = Vk::Initializers::PipelineColorBlendAttachmentState(
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = Vk::Initializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCreateInfo);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = Vk::Initializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = Vk::Initializers::PipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = Vk::Initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = Vk::Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables, 0);

    // Pipeline layout
    const std::vector<VkDescriptorSetLayout> setLayouts = {
        descriptorSetLayouts.scene, descriptorSetLayouts.material, descriptorSetLayouts.node
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Vk::Initializers::PipelineLayoutCreateInfo(setLayouts);
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.size = sizeof(PushConstBlockMaterial);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(vkDevice->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		Vk::Initializers::VertexInputBindingDescription(0, sizeof(Vk::GLTFModel::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		Vk::Initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, pos)),
		Vk::Initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, normal)),		
        Vk::Initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, uv0)),
        Vk::Initializers::VertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, uv1)),
        Vk::Initializers::VertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, joint0)),
        Vk::Initializers::VertexInputAttributeDescription(0, 5, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, weight0)),
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = Vk::Initializers::PipelineVertexInputStateCreateInfo();
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();

	const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
		LoadShader(GetAssetPath() + "shaders/compiled/glTF_PBR.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		LoadShader(GetAssetPath() + "shaders/compiled/glTF_PBR.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vk::Initializers::PipelineCreateInfo(pipelineLayout, renderPass, 0);
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();

    // multisampleStateCreateInfo.rasterizationSamples = sampleCount;
    // multisampleStateCreateInfo.sampleShadingEnable = VK_TRUE;
    // multisampleStateCreateInfo.minSampleShading = 0.25f;

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(vkDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solidPBR_MSAA));

	// Wire frame rendering pipeline
    if (vkDevice->enabledFeatures.fillModeNonSolid) {
        VkPipelineRasterizationStateCreateInfo wireframeRasterizationCI = rasterizationStateCreateInfo;
        wireframeRasterizationCI.polygonMode = VK_POLYGON_MODE_LINE;
        wireframeRasterizationCI.lineWidth = 1.0f;
        pipelineCreateInfo.pRasterizationState = &wireframeRasterizationCI;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vkDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));
        pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	}
    else
    {
        pipelines.wireframe = pipelines.solidPBR_MSAA;
    }

    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    blendAttachmentStateCreateInfo.blendEnable = VK_TRUE;
    blendAttachmentStateCreateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachmentStateCreateInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentStateCreateInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentStateCreateInfo.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentStateCreateInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentStateCreateInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentStateCreateInfo.alphaBlendOp = VK_BLEND_OP_ADD;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(vkDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solidPBR_MSAA_AlphaBlend));

    for (auto shaderStage : shaderStages) {
        vkDestroyShaderModule(vkDevice->logicalDevice, shaderStage.module, nullptr);
    }
}

void CG::EngineImpl::BuildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = nullptr;

	std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = { uiData.bgColor.r, uiData.bgColor.g, uiData.bgColor.b, uiData.bgColor.a };
    // clearValues[1].color = { uiData.bgColor.r, uiData.bgColor.g, uiData.bgColor.b, uiData.bgColor.a };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = engineConfig.width;
	renderPassBeginInfo.renderArea.extent.height = engineConfig.height;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues = clearValues.data();

	BuildUiCommandBuffers();

	VkViewport viewport = {};
	viewport.height = static_cast<float>(engineConfig.height);
	viewport.width = static_cast<float>(engineConfig.width);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent.width = engineConfig.width;
	scissor.extent.height = engineConfig.height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;

    // VkDeviceSize offsets[1] = { 0 };

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		renderPassBeginInfo.framebuffer = frameBuffers[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

        /*
		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

        if (testSkybox)
        {
            // all pipelines placed inside class
            testSkybox->Draw(drawCmdBuffers[i]);
        }
        */

        /*
        if (testScene && testScene->IsLoaded())
        {
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solidPBR_MSAA);

            vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &testScene->vertices.buffer.buffer, offsets);
            if (testScene->indices.buffer.buffer != VK_NULL_HANDLE) {
                vkCmdBindIndexBuffer(drawCmdBuffers[i], testScene->indices.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            }

            // Opaque primitives first
            for (const auto& node : testScene->GetNodes()) {
                RenderNode(node.get(), i, Vk::GLTFModel::Material::eAlphaMode::kAlphaModeOpaque);
            }
            // Alpha masked primitives
            for (const auto& node : testScene->GetNodes()) {
                RenderNode(node.get(), i, Vk::GLTFModel::Material::eAlphaMode::kAlphaModeMask);
            }
            // Transparent primitives
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solidPBR_MSAA_AlphaBlend);
            for (const auto& node : testScene->GetNodes()) {
                RenderNode(node.get(), i, Vk::GLTFModel::Material::eAlphaMode::kAlphaModeBlend);
            }
        }
        */

        DrawRayTracingData(i);

        vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
		
        imGui->DrawFrame(drawCmdBuffers[i]);

		vkCmdEndRenderPass(drawCmdBuffers[i]);

        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void CG::EngineImpl::SetupDescriptors()
{
    assert(testScene != nullptr);

    constexpr uint32_t uniformSamplersCount = 5;

    uint32_t imageSamplerCount = 0;
    uint32_t materialCount = 0;
    uint32_t meshCount = 0;

    // Environment samplers (env map)
    imageSamplerCount += 1;

    materialCount = static_cast<uint32_t>(testScene->GetMaterials().size());
    imageSamplerCount += materialCount * uniformSamplersCount;

    for (auto& node : testScene->GetFlatNodes()) {
        if (node->mesh) {
            meshCount++;
        }
    }

    std::vector<VkDescriptorPoolSize> poolSizes = 
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 /*env map*/ + meshCount },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamplerCount }
    };
    VkDescriptorPoolCreateInfo descriptorPoolCI = Vk::Initializers::DescriptorPoolCreateInfo(poolSizes, 2 + materialCount + meshCount);
    VK_CHECK_RESULT(vkCreateDescriptorPool(vkDevice->logicalDevice, &descriptorPoolCI, nullptr, &descriptorPool));


    // matrices
    // TODO: IBL samplers
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        };
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
        descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
        descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.scene));

        VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.descriptorPool = descriptorPool;
        descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayouts.scene;
        descriptorSetAllocInfo.descriptorSetCount = 1;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &descriptorSetAllocInfo, &descriptorSets.scene));

        std::array<VkWriteDescriptorSet, 1> writeDescriptorSets{};

        writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSets[0].descriptorCount = 1;
        writeDescriptorSets[0].dstSet = descriptorSets.scene;
        writeDescriptorSets[0].dstBinding = 0;
        writeDescriptorSets[0].pBufferInfo = &sceneUbo.descriptor;

        /*
        writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescriptorSets[2].descriptorCount = 1;
        writeDescriptorSets[2].dstSet = descriptorSets.scene;
        writeDescriptorSets[2].dstBinding = 2;
        writeDescriptorSets[2].pImageInfo = &textures.irradianceCube.descriptor;
        */

        vkUpdateDescriptorSets(vkDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    }

    // Material (samplers)
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
            { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        };
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
        descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
        descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.material));

        // Per-Material descriptor sets
        for (auto& material : testScene->GetMaterials()) {
            VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
            descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocInfo.descriptorPool = descriptorPool;
            descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayouts.material;
            descriptorSetAllocInfo.descriptorSetCount = 1;
            VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &descriptorSetAllocInfo, &material.descriptorSet));

            std::vector<VkDescriptorImageInfo> imageDescriptors = {
                emptyTexture.descriptor,
                emptyTexture.descriptor,
                material.normalTexture ? material.normalTexture->texture.descriptor : emptyTexture.descriptor,
                material.occlusionTexture ? material.occlusionTexture->texture.descriptor : emptyTexture.descriptor,
                material.emissiveTexture ? material.emissiveTexture->texture.descriptor : emptyTexture.descriptor,
            };

            // TODO: spec gloss
            if (material.pbrWorkflows.metallicRoughness || material.pbrWorkflows.specularGlossiness) {
                if (material.baseColorTexture) {
                    imageDescriptors[0] = material.baseColorTexture->texture.descriptor;
                }
                if (material.metallicRoughnessTexture) {
                    imageDescriptors[1] = material.metallicRoughnessTexture->texture.descriptor;
                }
            }

            std::array<VkWriteDescriptorSet, uniformSamplersCount> writeDescriptorSets{};
            for (size_t i = 0; i < imageDescriptors.size(); i++) {
                writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writeDescriptorSets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writeDescriptorSets[i].descriptorCount = 1;
                writeDescriptorSets[i].dstSet = material.descriptorSet;
                writeDescriptorSets[i].dstBinding = static_cast<uint32_t>(i);
                writeDescriptorSets[i].pImageInfo = &imageDescriptors[i];
            }

            vkUpdateDescriptorSets(vkDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
        }

        // Model node (matrices)
        {
            std::vector<VkDescriptorSetLayoutBinding> modelSetLayoutBindings = {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
            };
            VkDescriptorSetLayoutCreateInfo modelDescriptorSetLayoutCI = Vk::Initializers::DescriptorSetLayoutCreateInfo(modelSetLayoutBindings);
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &modelDescriptorSetLayoutCI, nullptr, &descriptorSetLayouts.node));

            // Per-Node descriptor set
            for (const auto& node : testScene->GetNodes()) {
                SetupNodeDescriptorSet(node.get());
            }
        }

    }
}

void CG::EngineImpl::BindModelMaterials()
{
    SetupDescriptors();
}

void CG::EngineImpl::UnbindModelMaterials()
{

}

void CG::EngineImpl::PrepareUniformBuffers()
{
	auto cameraEntity = registry.create();
	CameraComponent& component = registry.assign<CameraComponent>(cameraEntity);

	component.vkDevice = vkDevice;
	// component.uniformBufferVS.PrepareUniformBuffers(vkDevice, sizeof(component.uboVS));
	component.viewport.height = engineConfig.height;
	component.viewport.width = engineConfig.width;

	uniformBufferVS = &component.uniformBufferVS;

	cameraComponent = &component;

	VK_CHECK_RESULT(vkDevice->CreateBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&ubo,
		sizeof(uboData)));

	// Map persistent
	VK_CHECK_RESULT(ubo.Map());

	VK_CHECK_RESULT(vkDevice->CreateBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&sceneUbo,
		sizeof(sceneUboData)));

	// Map persistent
	VK_CHECK_RESULT(sceneUbo.Map());
}

void CG::EngineImpl::UpdateUniformBuffers()
{
	uboData.projection = cameraComponent->uboVS.projectionMatrix;
	uboData.view = cameraComponent->uboVS.viewMatrix;
	uboData.lightPosPushConstants.fill(glm::vec4(0.0f));
	uboData.lightColorPushConstants.fill(glm::vec4(0.0f));

    uboData.lightPosPushConstants[0] = glm::vec4(1.5f, 1.5f, -1.5f, 0.0f);
    uboData.lightColorPushConstants[0] = glm::vec4(23.47f, 21.31f, 20.79f, 0.0f);

    uboData.lightPosPushConstants[1] = glm::vec4(-1.5f, 1.0f, -1.0f, 0.0f);
    uboData.lightColorPushConstants[1] = glm::vec4(16.47f, 11.31f, 22.79f, 0.0f) * 0.1f;

    uboData.lightPosPushConstants[2] = glm::vec4(0.1f, 2.5f, -1.5f, 0.0f);
    uboData.lightColorPushConstants[2] = glm::vec4(13.47f, 25.31f, 23.79f, 0.0f) * 0.03f;

    uboData.lightPosPushConstants[3] = glm::vec4(1.5f, -1.5f, 1.5f, 0.0f);
    uboData.lightColorPushConstants[3] = glm::vec4(14.47f, 11.31f, 27.79f, 0.0f) * 0.04f;

	ubo.CopyTo(&uboData, sizeof(uboData));


    for (size_t i = 0; i < uboData.lightPosPushConstants.size(); ++i)
    {
        sceneUboData.lightPosPushConstants[i] = uboData.lightPosPushConstants[i];
        sceneUboData.lightColorPushConstants[i] = uboData.lightColorPushConstants[i];
    }

    sceneUboData.projection = cameraComponent->uboVS.projectionMatrix;
    sceneUboData.view = cameraComponent->uboVS.viewMatrix;

    float scale = (1.0f / std::max(testScene->GetSize().x, std::max(testScene->GetSize().y, testScene->GetSize().z))) * 0.5f;

    sceneUboData.model = glm::mat4(1.0f);
    sceneUboData.model[0][0] = scale;
    sceneUboData.model[1][1] = scale;
    sceneUboData.model[2][2] = scale;

    sceneUboData.cameraPos = glm::vec4(cameraComponent->position, 1.0f);

    sceneUbo.CopyTo(&sceneUboData, sizeof(sceneUboData));

	if (testSkybox)
	{
		testSkybox->UpdateCameraUniformBuffer(cameraComponent);
	}
}

void CG::EngineImpl::SetupSystems()
{
	auto cameraSystem = std::make_unique<CameraSystem>();
	cameraSystem->SetDevice(vkDevice);
	systems.push_back(std::move(cameraSystem));

	auto lightSystem = std::make_unique<LightSystem>();
	systems.push_back(std::move(lightSystem));
}

void CG::EngineImpl::DrawUI()
{
	ImGui::NewFrame();

	if (uiData.isActive)
	{
		// Init imGui windows and elements
		ImGui::Begin("Coldgaze overlay", &uiData.isActive, ImGuiWindowFlags_MenuBar);

		// Edit a color (stored as ~4 floats)
		ImGui::ColorEdit4("Background", &uiData.bgColor.x, ImGuiColorEditFlags_NoInputs);

		ImGui::Checkbox("Wireframe mode", &uiData.drawWire);

		// Plot some values
		const float dummyData[] = { 0.2f, 0.1f, 1.0f, 0.5f, 0.9f, 2.2f };
		ImGui::PlotLines("Frame Times", dummyData, IM_ARRAYSIZE(dummyData));

		ImGui::SliderFloat("Camera FOV:", &uiData.fov, 10.0f, 135.0f);
		cameraComponent->fov = uiData.fov;

		ImGui::End();
	}

	{
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::MenuItem("File menu", nullptr, false, false);
				{
					if (ImGui::MenuItem("Open scene", "*.gltf"))
					{
						nfdchar_t* outPath = nullptr;
# pragma warning(push)
# pragma warning(disable : 26812)
						nfdresult_t result = NFD_OpenDialog(nullptr, nullptr, &outPath);
# pragma warning(pop)
						if (result == NFD_OKAY) {
							LoadModelAsync(outPath);
							free(outPath);
						}
						else if (result == NFD_ERROR)
						{
							std::cerr << NFD_GetError() << std::endl;
						}
					}

                    if (ImGui::MenuItem("Set skybox", "*.hdr"))
                    {
                        nfdchar_t* outPath = nullptr;
# pragma warning(push)
# pragma warning(disable : 26812)
                        nfdresult_t result = NFD_OpenDialog(nullptr, nullptr, &outPath);
# pragma warning(pop)
                        if (result == NFD_OKAY) {
                            LoadSkybox(outPath);
                            free(outPath);
                        }
                        else if (result == NFD_ERROR)
                        {
                            std::cerr << NFD_GetError() << std::endl;
                        }
                    }
					ImGui::EndMenu();
				}
            }
            if (ImGui::BeginMenu("Overlay"))
            {
				if (ImGui::MenuItem("Show", nullptr, false, !uiData.isActive)) { uiData.isActive = true; }
				if (ImGui::MenuItem("Hide", nullptr, false, uiData.isActive)) { uiData.isActive = false; }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
	}

	// Render to generate draw buffers
	ImGui::Render();
}

void CG::EngineImpl::BuildUiCommandBuffers()
{
	DrawUI();

	imGui->UpdateBuffers();
}

void CG::EngineImpl::LoadModel(const std::string& modelFilePath)
{
	{
        if (testScene)
        {
            UnbindModelMaterials();
        }

        testScene = std::make_unique<Vk::GLTFModel>();

        testScene->vkDevice = vkDevice;
        testScene->queue = queue;
	}

	testScene->LoadFromFile(modelFilePath);
}

void CG::EngineImpl::LoadModelAsync(const std::string& modelFilePath)
{
    try
    {
        LoadModel(modelFilePath);

        BindModelMaterials();
        testScene->SetLoaded(true);

        CreateNVRayTracingGeometry();
    }
    catch (const Vk::AssetLoadingException& e)
    {
		std::cerr << e.what() << std::endl;

        const SDL_MessageBoxButtonData buttons[] = {
			{ /* .flags, .buttonid, .text */        0, 0, "ok" },
        };
        const SDL_MessageBoxData messageboxdata = {
            SDL_MESSAGEBOX_INFORMATION,
            NULL,
            "Asset loading error",
			e.what(),
            SDL_arraysize(buttons),
            buttons,
        };
        if (SDL_ShowMessageBox(&messageboxdata, nullptr) < 0) {
			throw std::runtime_error("Error while trying to display SDL message box:" + std::string(SDL_GetError()));
        }
    }
}

void CG::EngineImpl::LoadSkybox(const std::string& cubeMapFilePath)
{
	try
	{
        testSkybox = std::make_unique<Vk::SkyBox>(*this);
        testSkybox->LoadFromFile(cubeMapFilePath, vkDevice, queue);
		testSkybox->SetupDescriptorSet(descriptorPool);
        testSkybox->PreparePipeline(renderPass, pipelineCache);
	}
    catch (const Vk::AssetLoadingException & e)
    {
		testSkybox = nullptr;

        std::cerr << e.what() << std::endl;

        const SDL_MessageBoxButtonData buttons[] = {
            { /* .flags, .buttonid, .text */        0, 0, "ok" },
        };
        const SDL_MessageBoxData messageboxdata = {
            SDL_MESSAGEBOX_INFORMATION,
            NULL,
            "Asset loading error",
            e.what(),
            SDL_arraysize(buttons),
            buttons,
        };
        if (SDL_ShowMessageBox(&messageboxdata, nullptr) < 0) {
            throw std::runtime_error("Error while trying to display SDL message box:" + std::string(SDL_GetError()));
        }
    }
}

void CG::EngineImpl::RenderNode(Vk::GLTFModel::Node* node, uint32_t cbIndex, Vk::GLTFModel::Material::eAlphaMode alphaMode)
{
    enum class ePBRWorkflows { kPbrMetalRough = 0, kPbrSpecGloss = 1 };

    if (node->mesh) {
        // Render mesh primitives
        for (const auto& primitive : node->mesh->primitives) {
            if (primitive->material.alphaMode == alphaMode) {

                const std::vector<VkDescriptorSet> descriptorsets = {
                    descriptorSets.scene,
                    primitive->material.descriptorSet,
                    node->mesh->uniformBuffer.descriptorSet,
                };
                vkCmdBindDescriptorSets(drawCmdBuffers[cbIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorsets.size()), descriptorsets.data(), 0, NULL);

                // Pass material parameters as push constants
                PushConstBlockMaterial pushConstBlockMaterial = {};
                pushConstBlockMaterial.emissiveFactor = primitive->material.emissiveFactor;
                pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
                pushConstBlockMaterial.normalTextureSet = primitive->material.normalTexture != nullptr ? primitive->material.texCoordSets.normal : -1;
                pushConstBlockMaterial.occlusionTextureSet = primitive->material.occlusionTexture != nullptr ? primitive->material.texCoordSets.occlusion : -1;
                pushConstBlockMaterial.emissiveTextureSet = primitive->material.emissiveTexture != nullptr ? primitive->material.texCoordSets.emissive : -1;
                pushConstBlockMaterial.alphaMask = static_cast<float>(primitive->material.alphaMode == Vk::GLTFModel::Material::eAlphaMode::kAlphaModeMask);
                pushConstBlockMaterial.alphaMaskCutoff = primitive->material.alphaCutoff;


                if (primitive->material.pbrWorkflows.metallicRoughness) {
                    // Metallic roughness workflow
                    pushConstBlockMaterial.workflow = static_cast<float>(ePBRWorkflows::kPbrMetalRough);
                    pushConstBlockMaterial.baseColorFactor = primitive->material.baseColorFactor;
                    pushConstBlockMaterial.metallicFactor = primitive->material.metallicFactor;
                    pushConstBlockMaterial.roughnessFactor = primitive->material.roughnessFactor;
                    pushConstBlockMaterial.physicalDescriptorTextureSet = primitive->material.metallicRoughnessTexture != nullptr ? primitive->material.texCoordSets.metallicRoughness : -1;
                    pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
                }

                if (primitive->material.pbrWorkflows.specularGlossiness) {
                    // Specular glossiness workflow
                    // Not supported
                    pushConstBlockMaterial.workflow = static_cast<float>(ePBRWorkflows::kPbrSpecGloss);
                    pushConstBlockMaterial.baseColorFactor = primitive->material.baseColorFactor;
                    pushConstBlockMaterial.metallicFactor = primitive->material.metallicFactor;
                    pushConstBlockMaterial.roughnessFactor = primitive->material.roughnessFactor;
                    pushConstBlockMaterial.physicalDescriptorTextureSet = primitive->material.metallicRoughnessTexture != nullptr ? primitive->material.texCoordSets.metallicRoughness : -1;
                    pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
                }

                vkCmdPushConstants(drawCmdBuffers[cbIndex], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstBlockMaterial), &pushConstBlockMaterial);

                if (primitive->hasIndices) {
                    vkCmdDrawIndexed(drawCmdBuffers[cbIndex], primitive->indexCount, 1, primitive->firstIndex, 0, 0);
                }
                else {
                    vkCmdDraw(drawCmdBuffers[cbIndex], primitive->vertexCount, 1, 0, 0);
                }
            }
        }

    };
    for (const auto& child : node->children) {
        RenderNode(child.get(), cbIndex, alphaMode);
    }
}

void CG::EngineImpl::SetupNodeDescriptorSet(Vk::GLTFModel::Node* node)
{
    if (node->mesh) {
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.descriptorPool = descriptorPool;
        descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayouts.node;
        descriptorSetAllocInfo.descriptorSetCount = 1;
        VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &descriptorSetAllocInfo, &node->mesh->uniformBuffer.descriptorSet));

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.dstSet = node->mesh->uniformBuffer.descriptorSet;
        writeDescriptorSet.dstBinding = 0;
        writeDescriptorSet.pBufferInfo = &node->mesh->uniformBuffer.buffer.descriptor;

        vkUpdateDescriptorSets(vkDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
    }
    for (const auto& child : node->children) {
        SetupNodeDescriptorSet(child.get());
    }
}

void CG::EngineImpl::CreateBottomLevelAccelerationStructure(const VkGeometryNV* geometries, uint32_t blasIndex, size_t geomCount)
{
    AccelerationStructure& bottomLevelAS = blasData[blasIndex];

    // Prepare handle
    {
        VkAccelerationStructureInfoNV accelerationStructureInfo = {};
        accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        accelerationStructureInfo.instanceCount = 0;
        accelerationStructureInfo.geometryCount = static_cast<uint32_t>(geomCount);
        accelerationStructureInfo.pGeometries = geometries;

        VkAccelerationStructureCreateInfoNV accelerationStructureCI = {};
        accelerationStructureCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
        accelerationStructureCI.info = accelerationStructureInfo;
        VK_CHECK_RESULT(vkCreateAccelerationStructureNV(vkDevice->logicalDevice, &accelerationStructureCI, nullptr, &bottomLevelAS.accelerationStructure));

        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.accelerationStructure = bottomLevelAS.accelerationStructure;

        VkMemoryRequirements2 memoryRequirements2 = {};
        vkGetAccelerationStructureMemoryRequirementsNV(vkDevice->logicalDevice, &memoryRequirementsInfo, &memoryRequirements2);

        VkMemoryAllocateInfo memoryAllocateInfo = Vk::Initializers::MemoryAllocateInfo();
        memoryAllocateInfo.allocationSize = memoryRequirements2.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memoryAllocateInfo, nullptr, &bottomLevelAS.memory));

        VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo = {};
        accelerationStructureMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
        accelerationStructureMemoryInfo.accelerationStructure = bottomLevelAS.accelerationStructure;
        accelerationStructureMemoryInfo.memory = bottomLevelAS.memory;
        VK_CHECK_RESULT(vkBindAccelerationStructureMemoryNV(vkDevice->logicalDevice, 1, &accelerationStructureMemoryInfo));

        VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(vkDevice->logicalDevice, bottomLevelAS.accelerationStructure, sizeof(uint64_t), &bottomLevelAS.handle));

    }
   
    // Build step
    {
        // Acceleration structure build requires some scratch space to store temporary information
        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

        VkMemoryRequirements2 memReqBottomLevelAS;
        memoryRequirementsInfo.accelerationStructure = bottomLevelAS.accelerationStructure;
        vkGetAccelerationStructureMemoryRequirementsNV(vkDevice->logicalDevice, &memoryRequirementsInfo, &memReqBottomLevelAS);

        const VkDeviceSize scratchBufferSize = memReqBottomLevelAS.memoryRequirements.size;

        Vk::Buffer scratchBuffer;
        VK_CHECK_RESULT(vkDevice->CreateBuffer(
            VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &scratchBuffer,
            scratchBufferSize));

        VkCommandBuffer cmdBuffer = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkAccelerationStructureInfoNV buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        buildInfo.geometryCount = static_cast<uint32_t>(geomCount);
        buildInfo.pGeometries = geometries;

        vkCmdBuildAccelerationStructureNV(
            cmdBuffer,
            &buildInfo,
            VK_NULL_HANDLE,
            0,
            VK_FALSE,
            bottomLevelAS.accelerationStructure,
            VK_NULL_HANDLE,
            scratchBuffer.buffer,
            0);

        VkMemoryBarrier memoryBarrier = Vk::Initializers::CreateMemoryBarrier();
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

        vkDevice->FlushCommandBuffer(cmdBuffer, queue);
    }
}

void CG::EngineImpl::CreateTopLevelAccelerationStructure()
{
    {
        VkAccelerationStructureInfoNV accelerationStructureInfo = {};
        accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
        accelerationStructureInfo.instanceCount = static_cast<uint32_t>(blasData.size());
        accelerationStructureInfo.geometryCount = 0;

        VkAccelerationStructureCreateInfoNV accelerationStructureCI = {};
        accelerationStructureCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
        accelerationStructureCI.info = accelerationStructureInfo;
        VK_CHECK_RESULT(vkCreateAccelerationStructureNV(vkDevice->logicalDevice, &accelerationStructureCI, nullptr, &topLevelAS.accelerationStructure));

        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.accelerationStructure = topLevelAS.accelerationStructure;

        VkMemoryRequirements2 memoryRequirements2 = {};
        vkGetAccelerationStructureMemoryRequirementsNV(vkDevice->logicalDevice, &memoryRequirementsInfo, &memoryRequirements2);

        VkMemoryAllocateInfo memoryAllocateInfo = Vk::Initializers::MemoryAllocateInfo();
        memoryAllocateInfo.allocationSize = memoryRequirements2.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(memoryRequirements2.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memoryAllocateInfo, nullptr, &topLevelAS.memory));

        VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo = {};
        accelerationStructureMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
        accelerationStructureMemoryInfo.accelerationStructure = topLevelAS.accelerationStructure;
        accelerationStructureMemoryInfo.memory = topLevelAS.memory;
        VK_CHECK_RESULT(vkBindAccelerationStructureMemoryNV(vkDevice->logicalDevice, 1, &accelerationStructureMemoryInfo));

        VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(vkDevice->logicalDevice, topLevelAS.accelerationStructure, sizeof(uint64_t), &topLevelAS.handle));
    }

    {
        // Single instance with a 3x4 transform matrix for the ray traced triangle
        Vk::Buffer instanceBuffer;

        std::vector<GeometryInstance> geometryInstances(blasData.size());
        for (size_t i = 0; i < geometryInstances.size(); ++i)
        {
            GeometryInstance& geometryInstance = geometryInstances[i];

            geometryInstance.transform = glm::transpose(blasData[i].transform);
            geometryInstance.instanceId = i;
            geometryInstance.mask = 0xff;
            geometryInstance.instanceOffset = 0;
            geometryInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
            geometryInstance.accelerationStructureHandle = blasData[i].handle;
        }

        VK_CHECK_RESULT(vkDevice->CreateBuffer(
            VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &instanceBuffer,
            sizeof(GeometryInstance) * geometryInstances.size(),
            geometryInstances.data()));

        // Acceleration structure build requires some scratch space to store temporary information
        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo{};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

        VkMemoryRequirements2 memReqTopLevelAS;
        memoryRequirementsInfo.accelerationStructure = topLevelAS.accelerationStructure;
        vkGetAccelerationStructureMemoryRequirementsNV(vkDevice->logicalDevice, &memoryRequirementsInfo, &memReqTopLevelAS);

        const VkDeviceSize scratchBufferSize = memReqTopLevelAS.memoryRequirements.size;

        Vk::Buffer scratchBuffer;
        VK_CHECK_RESULT(vkDevice->CreateBuffer(
            VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &scratchBuffer,
            scratchBufferSize));

        VkCommandBuffer cmdBuffer = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkAccelerationStructureInfoNV buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
        buildInfo.pGeometries = 0;
        buildInfo.geometryCount = 0;
        buildInfo.instanceCount = static_cast<uint32_t>(blasData.size());

        vkCmdBuildAccelerationStructureNV(
            cmdBuffer,
            &buildInfo,
            instanceBuffer.buffer,
            0,
            VK_FALSE,
            topLevelAS.accelerationStructure,
            VK_NULL_HANDLE,
            scratchBuffer.buffer,
            0);

        VkMemoryBarrier memoryBarrier = Vk::Initializers::CreateMemoryBarrier();
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);

        vkDevice->FlushCommandBuffer(cmdBuffer, queue);

        instanceBuffer.Destroy();
    }
}

void CG::EngineImpl::LoadNVRayTracingProcs()
{
    VkDevice device = vkDevice->logicalDevice;

    // Get VK_NV_ray_tracing related function pointers
    vkCreateAccelerationStructureNV = reinterpret_cast<PFN_vkCreateAccelerationStructureNV>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureNV"));
    vkDestroyAccelerationStructureNV = reinterpret_cast<PFN_vkDestroyAccelerationStructureNV>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureNV"));
    vkBindAccelerationStructureMemoryNV = reinterpret_cast<PFN_vkBindAccelerationStructureMemoryNV>(vkGetDeviceProcAddr(device, "vkBindAccelerationStructureMemoryNV"));
    vkGetAccelerationStructureHandleNV = reinterpret_cast<PFN_vkGetAccelerationStructureHandleNV>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureHandleNV"));
    vkGetAccelerationStructureMemoryRequirementsNV = reinterpret_cast<PFN_vkGetAccelerationStructureMemoryRequirementsNV>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureMemoryRequirementsNV"));
    vkCmdBuildAccelerationStructureNV = reinterpret_cast<PFN_vkCmdBuildAccelerationStructureNV>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructureNV"));
    vkCreateRayTracingPipelinesNV = reinterpret_cast<PFN_vkCreateRayTracingPipelinesNV>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesNV"));
    vkGetRayTracingShaderGroupHandlesNV = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesNV>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesNV"));
    vkCmdTraceRaysNV = reinterpret_cast<PFN_vkCmdTraceRaysNV>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysNV"));
}

void CG::EngineImpl::CreateNVRayTracingGeometry()
{
    assert(testScene);

    blasData.resize(testScene->GetPrimitivesCount());
    std::vector<VkGeometryNV> geometries(testScene->GetPrimitivesCount());
    uint32_t currentGeomIndex = 0;

    for (const auto& node : testScene->GetFlatNodes())
    {
        if (node->mesh)
        {
            for (const auto& primitive : node->mesh->primitives)
            {
                VkGeometryNV& geometry = geometries[currentGeomIndex];
                geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
                geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
                geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
                geometry.geometry.triangles.vertexData = primitive->vertices.buffer;
                geometry.geometry.triangles.vertexOffset = 0;
                geometry.geometry.triangles.vertexCount = primitive->vertexCount;
                geometry.geometry.triangles.vertexStride = sizeof(Vk::GLTFModel::Vertex);
                geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                geometry.geometry.triangles.indexData = primitive->indices.buffer;
                geometry.geometry.triangles.indexOffset = 0;
                geometry.geometry.triangles.indexCount = primitive->indexCount;
                geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
                geometry.geometry.triangles.transformOffset = 0;
                geometry.geometry.aabbs = {};
                geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
                geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;

                blasData[currentGeomIndex].transform = node->GetWorldMatrix();
                CreateBottomLevelAccelerationStructure(&geometry, currentGeomIndex, 1);

                ++currentGeomIndex;
            }
        }
    }

    CreateTopLevelAccelerationStructure();
}

// TODO: Handle on resize
void CG::EngineImpl::CreateNVRayTracingStoreImage()
{
    VkImageCreateInfo image = Vk::Initializers::ImageCreateInfo();
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = vkSwapChain->colorFormat;
    image.extent.width =  engineConfig.width;
    image.extent.height = engineConfig.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(vkDevice->logicalDevice, &image, nullptr, &storageImage.image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(vkDevice->logicalDevice, storageImage.image, &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo = Vk::Initializers::MemoryAllocateInfo();
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memoryAllocateInfo, nullptr, &storageImage.memory));
    VK_CHECK_RESULT(vkBindImageMemory(vkDevice->logicalDevice, storageImage.image, storageImage.memory, 0));

    VkImageViewCreateInfo colorImageView = Vk::Initializers::ImageViewCreateInfo();
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.format = vkSwapChain->colorFormat;
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.image = storageImage.image;
    VK_CHECK_RESULT(vkCreateImageView(vkDevice->logicalDevice, &colorImageView, nullptr, &storageImage.view));

    VkCommandBuffer cmdBuffer = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Vk::Utils::SetImageLayout(cmdBuffer, storageImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    vkDevice->FlushCommandBuffer(cmdBuffer, queue);
}

void CG::EngineImpl::CreateShaderBindingTable()
{
    const uint32_t sbtSize = rayTracingProperties.shaderGroupHandleSize * 3;
    VK_CHECK_RESULT(vkDevice->CreateBuffer(
        VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &shaderBindingTable,
        sbtSize));
    shaderBindingTable.Map();

    auto shaderHandleStorage = new uint8_t[sbtSize];
    // Get shader identifiers
    VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesNV(vkDevice->logicalDevice, pipelines.RTX, 0, 3, sbtSize, shaderHandleStorage));
    auto* data = static_cast<uint8_t*>(shaderBindingTable.mapped);

    // Copy the shader identifiers to the shader binding table
    data += CopyShaderIdentifier(data, shaderHandleStorage, kIndexRaygen);
    data += CopyShaderIdentifier(data, shaderHandleStorage, kIndexMiss);
    data += CopyShaderIdentifier(data, shaderHandleStorage, kIndexClosestHit);
    shaderBindingTable.Unmap();
}

VkDeviceSize CG::EngineImpl::CopyShaderIdentifier(uint8_t* data, const uint8_t* shaderHandleStorage, uint32_t groupIndex)
{
    const uint32_t shaderGroupHandleSize = rayTracingProperties.shaderGroupHandleSize;
    memcpy(data, shaderHandleStorage + groupIndex * shaderGroupHandleSize, shaderGroupHandleSize);
    return shaderGroupHandleSize;
}

void CG::EngineImpl::CreateRTXPipeline()
{
    VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
    accelerationStructureLayoutBinding.binding = 0;
    accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    VkDescriptorSetLayoutBinding resultImageLayoutBinding{};
    resultImageLayoutBinding.binding = 1;
    resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    resultImageLayoutBinding.descriptorCount = 1;
    resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    VkDescriptorSetLayoutBinding uniformBufferBinding{};
    uniformBufferBinding.binding = 2;
    uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBufferBinding.descriptorCount = 1;
    uniformBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        accelerationStructureLayoutBinding,
        resultImageLayoutBinding,
        uniformBufferBinding
        });

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &layoutInfo, nullptr, &descriptorSetLayouts.rtxSampleLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.rtxSampleLayout;

    VK_CHECK_RESULT(vkCreatePipelineLayout(vkDevice->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

    const uint32_t shaderIndexRaygen = 0;
    const uint32_t shaderIndexMiss = 1;
    const uint32_t shaderIndexClosestHit = 2;

    std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;
    shaderStages[shaderIndexRaygen] = LoadShader(GetAssetPath() + "shaders/compiled/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_NV);
    shaderStages[shaderIndexMiss] = LoadShader(GetAssetPath() + "shaders/compiled/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_NV);
    shaderStages[shaderIndexClosestHit] = LoadShader(GetAssetPath() + "shaders/compiled/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

    std::array<VkRayTracingShaderGroupCreateInfoNV, 3> groups{};
    for (auto& group : groups) {
        // Init all groups with some default values
        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
        group.generalShader = VK_SHADER_UNUSED_NV;
        group.closestHitShader = VK_SHADER_UNUSED_NV;
        group.anyHitShader = VK_SHADER_UNUSED_NV;
        group.intersectionShader = VK_SHADER_UNUSED_NV;
    }

    // Links shaders and types to ray tracing shader groups
    groups[kIndexRaygen].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    groups[kIndexRaygen].generalShader = shaderIndexRaygen;
    groups[kIndexMiss].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
    groups[kIndexMiss].generalShader = shaderIndexMiss;
    groups[kIndexClosestHit].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
    groups[kIndexClosestHit].generalShader = VK_SHADER_UNUSED_NV;
    groups[kIndexClosestHit].closestHitShader = shaderIndexClosestHit;

    VkRayTracingPipelineCreateInfoNV rayPipelineInfo{};
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayPipelineInfo.pStages = shaderStages.data();
    rayPipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    rayPipelineInfo.pGroups = groups.data();
    rayPipelineInfo.maxRecursionDepth = 1;
    rayPipelineInfo.layout = pipelineLayout;

    VK_CHECK_RESULT(vkCreateRayTracingPipelinesNV(vkDevice->logicalDevice, VK_NULL_HANDLE, 1, &rayPipelineInfo, nullptr, &pipelines.RTX));
}

void CG::EngineImpl::CreateRTXDescriptorSets()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = Vk::Initializers::DescriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(vkDevice->logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = Vk::Initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.rtxSampleLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &descriptorSetAllocateInfo, &descriptorSets.rtxSample));

    VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo{};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.accelerationStructure;

    VkWriteDescriptorSet accelerationStructureWrite{};
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // The specialized acceleration structure descriptor has to be chained
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = descriptorSets.rtxSample;
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

    VkDescriptorImageInfo storageImageDescriptor{};
    storageImageDescriptor.imageView = storageImage.view;
    storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet resultImageWrite = Vk::Initializers::WriteDescriptorSet(descriptorSets.rtxSample, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
    VkWriteDescriptorSet uniformBufferWrite = Vk::Initializers::WriteDescriptorSet(descriptorSets.rtxSample, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &sceneUbo.descriptor);

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        accelerationStructureWrite,
        resultImageWrite,
        uniformBufferWrite,
    };
    vkUpdateDescriptorSets(vkDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void CG::EngineImpl::DrawRayTracingData(uint32_t swapChainImageIndex)
{
    const VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdBindPipeline(drawCmdBuffers[swapChainImageIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipelines.RTX);
    vkCmdBindDescriptorSets(drawCmdBuffers[swapChainImageIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipelineLayout, 0, 1, &descriptorSets.rtxSample, 0, 0);

    VkDeviceSize bindingOffsetRayGenShader = rayTracingProperties.shaderGroupHandleSize * kIndexRaygen;
    VkDeviceSize bindingOffsetMissShader = rayTracingProperties.shaderGroupHandleSize * kIndexMiss;
    VkDeviceSize bindingOffsetHitShader = rayTracingProperties.shaderGroupHandleSize * kIndexClosestHit;
    VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;

    vkCmdTraceRaysNV(drawCmdBuffers[swapChainImageIndex],
        shaderBindingTable.buffer, bindingOffsetRayGenShader,
        shaderBindingTable.buffer, bindingOffsetMissShader, bindingStride,
        shaderBindingTable.buffer, bindingOffsetHitShader, bindingStride,
        VK_NULL_HANDLE, 0, 0,
        engineConfig.width, engineConfig.height, 1);

    // Prepare current swapchain image as transfer destination
    Vk::Utils::SetImageLayout(
        drawCmdBuffers[swapChainImageIndex],
        vkSwapChain->images[swapChainImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    // Prepare ray tracing output image as transfer source
    Vk::Utils::SetImageLayout(
        drawCmdBuffers[swapChainImageIndex],
        storageImage.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        subresourceRange);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.srcOffset = { 0, 0, 0 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstOffset = { 0, 0, 0 };
    copyRegion.extent = { engineConfig.width, engineConfig.height, 1 };
    vkCmdCopyImage(drawCmdBuffers[swapChainImageIndex], storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
        vkSwapChain->images[swapChainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // Transition swap chain image back for presentation
    Vk::Utils::SetImageLayout(
        drawCmdBuffers[swapChainImageIndex],
        vkSwapChain->images[swapChainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        subresourceRange);

    // Transition ray tracing output image back to general layout
    Vk::Utils::SetImageLayout(
        drawCmdBuffers[swapChainImageIndex],
        storageImage.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        subresourceRange);
}
