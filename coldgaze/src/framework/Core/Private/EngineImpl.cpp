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

using namespace CG;

constexpr float DEFAULT_FOV = 60.0f;

CG::EngineImpl::EngineImpl(CG::EngineConfig& engineConfig)
    : CG::Engine(engineConfig)
{ }

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

	SetupSystems();

	PrepareUniformBuffers();
	SetupDescriptors();

	// LoadSkybox(GetAssetPath() + "textures/hdr/Malibu_Overlook_3k.hdr");
	LoadModelAsync(GetAssetPath() + "models/FlightHelmet/glTF/FlightHelmet.gltf");

	PreparePipelines();
	BuildCommandBuffers();
}

void CG::EngineImpl::Cleanup()
{
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
				cameraComponent->position = glm::vec3(0.0f, 0.0f, 1.0f);
				cameraComponent->rotation = glm::vec3(180.0f, 0.0f, 0.0f);
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
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = Vk::Initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCreateInfo = Vk::Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = Vk::Initializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCreateInfo);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = Vk::Initializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = Vk::Initializers::PipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = Vk::Initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = Vk::Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables, 0);

	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		Vk::Initializers::VertexInputBindingDescription(0, sizeof(Vk::GLTFModel::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		Vk::Initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, pos)),	// Location 0: Position	
		Vk::Initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, normal)),// Location 1: Normal
		Vk::Initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, uv)),	// Location 2: Texture coordinates
		Vk::Initializers::VertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vk::GLTFModel::Vertex, color)),	// Location 3: Color
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = Vk::Initializers::PipelineVertexInputStateCreateInfo();
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();

	const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
		LoadShader(GetAssetPath() + "shaders/compiled/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		LoadShader(GetAssetPath() + "shaders/compiled/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = Vk::Initializers::PipelineCreateInfo(pipelineLayout, renderPass, 0);
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCI;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();

    multisampleStateCreateInfo.rasterizationSamples = sampleCount;
    multisampleStateCreateInfo.sampleShadingEnable = VK_TRUE;
    multisampleStateCreateInfo.minSampleShading = 0.25f;

	blendAttachmentStateCreateInfo.blendEnable = VK_TRUE;
	blendAttachmentStateCreateInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentStateCreateInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;

	blendAttachmentStateCreateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentStateCreateInfo.blendEnable = VK_TRUE;
	blendAttachmentStateCreateInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentStateCreateInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentStateCreateInfo.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentStateCreateInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentStateCreateInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachmentStateCreateInfo.alphaBlendOp = VK_BLEND_OP_ADD;

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(vkDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.solidPBR_MSAA));

	// Wire frame rendering pipeline
	if (vkDevice->enabledFeatures.fillModeNonSolid) {
		rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationStateCreateInfo.lineWidth = 1.0f;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vkDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));
	}
}

void CG::EngineImpl::BuildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = nullptr;

	std::array<VkClearValue, 3> clearValues;
    clearValues[0].color = { uiData.bgColor.r, uiData.bgColor.g, uiData.bgColor.b, uiData.bgColor.a };
    clearValues[1].color = { uiData.bgColor.r, uiData.bgColor.g, uiData.bgColor.b, uiData.bgColor.a };
	clearValues[2].depthStencil = { 1.0f, 0 };

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

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		renderPassBeginInfo.framebuffer = frameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

        if (testSkybox)
        {
            // all pipelines placed inside class
            testSkybox->Draw(drawCmdBuffers[i]);
        }

		if (testScene && testScene->IsLoaded())
		{
            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, uiData.drawWire ? pipelines.wireframe : pipelines.solidPBR_MSAA);

			testScene->Draw(drawCmdBuffers[i], pipelineLayout);
		}

		imGui->DrawFrame(drawCmdBuffers[i]);

		vkCmdEndRenderPass(drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void CG::EngineImpl::SetupDescriptors()
{
	// TODO: remove this constant, create separate descriptors for model;
	constexpr uint32_t kMaxImagesCount = 512;

	const std::vector<VkDescriptorPoolSize> poolSizes = 
	{
		Vk::Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		// One combined image sampler per model image/texture
        Vk::Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxImagesCount),
		/*Vk::Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
			static_cast<uint32_t>(testModel->GetImages().size())),*/
	};

	// One set for matrices and one per model image/texture
	const uint32_t maxSetCount = kMaxImagesCount + 1;

	VkDescriptorPoolCreateInfo descriptorPoolInfo = Vk::Initializers::DescriptorPoolCreateInfo(poolSizes, maxSetCount);
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
		Vk::Initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		Vk::Initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
	};
	VkDescriptorSetLayoutCreateInfo texturesDescriptorSetLayoutCreateInfo = Vk::Initializers::DescriptorSetLayoutCreateInfo(texturesSetLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &texturesDescriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayouts.textures));

	const std::vector<VkDescriptorSetLayout> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Vk::Initializers::PipelineLayoutCreateInfo(setLayouts);

	// For pushing primitive local matrices
	VkPushConstantRange pushConstantRange = Vk::Initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);

	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(vkDevice->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	const VkDescriptorSetAllocateInfo matricesAllocInfo = Vk::Initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &matricesAllocInfo, &descriptorSet));
	VkWriteDescriptorSet matricesWriteDescriptorSet = Vk::Initializers::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo.descriptor);
	vkUpdateDescriptorSets(vkDevice->logicalDevice, 1, &matricesWriteDescriptorSet, 0, nullptr);
}

void CG::EngineImpl::BindModelMaterials()
{
    // TODO: optimize storage in images
    std::vector<Vk::GLTFModel::Image>& images = testScene->GetImages();
    const std::vector<Vk::GLTFModel::Texture>& sceneTextures = testScene->GetTextures();

    for (auto& material : testScene->GetMaterials())
    {
        auto& baseColorImage = images[sceneTextures[material.baseColorTextureIndex].imageIndex];
        auto& normalMapImage = images[sceneTextures[material.normalMapTextureIndex].imageIndex];
        auto& metallicRoughnessImage = images[sceneTextures[material.metallicRoughnessTextureIndex].imageIndex];

        const VkDescriptorSetAllocateInfo texturesAllocInfo = Vk::Initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);

        VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &texturesAllocInfo, &material.descriptorSet));

        std::vector<VkWriteDescriptorSet> texturesWriteDescriptorSets =
        {
            Vk::Initializers::WriteDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &baseColorImage.texture->descriptor),
            Vk::Initializers::WriteDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMapImage.texture->descriptor),
            Vk::Initializers::WriteDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &metallicRoughnessImage.texture->descriptor),
        };

        vkUpdateDescriptorSets(vkDevice->logicalDevice, static_cast<uint32_t>(texturesWriteDescriptorSets.size()), texturesWriteDescriptorSets.data(), 0, nullptr);
    }
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

	UpdateUniformBuffers();
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

        {
            BindModelMaterials();
            testScene->SetLoaded(true);
        }
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
        GenerateIrradianceSampler();
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

void CG::EngineImpl::GenerateIrradianceSampler()
{
    assert(testSkybox);

    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    const int32_t dim = 64;
    const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    // Pre-filtered cube map
    // Image
    VkImageCreateInfo imageCI = Vk::Initializers::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = numMips;
    imageCI.arrayLayers = 6;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    VK_CHECK_RESULT(vkCreateImage(vkDevice->logicalDevice, &imageCI, nullptr, &textures.irradianceCube.image));

    VkMemoryAllocateInfo irradianceCubeMemAlloc = Vk::Initializers::MemoryAllocateInfo();
    VkMemoryRequirements irradianceCubeMemReqs;
    vkGetImageMemoryRequirements(vkDevice->logicalDevice, textures.irradianceCube.image, &irradianceCubeMemReqs);
    irradianceCubeMemAlloc.allocationSize = irradianceCubeMemReqs.size;
    irradianceCubeMemAlloc.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(irradianceCubeMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &irradianceCubeMemAlloc, nullptr, &textures.irradianceCube.deviceMemory));
    VK_CHECK_RESULT(vkBindImageMemory(vkDevice->logicalDevice, textures.irradianceCube.image, textures.irradianceCube.deviceMemory, 0));

    // Image view
    VkImageViewCreateInfo viewCI = Vk::Initializers::ImageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = numMips;
    viewCI.subresourceRange.layerCount = 6;
    viewCI.image = textures.irradianceCube.image;
    VK_CHECK_RESULT(vkCreateImageView(vkDevice->logicalDevice, &viewCI, nullptr, &textures.irradianceCube.view));
    // Sampler
    VkSamplerCreateInfo samplerCI = Vk::Initializers::SamplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = static_cast<float>(numMips);
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(vkDevice->logicalDevice, &samplerCI, nullptr, &textures.irradianceCube.sampler));

    textures.irradianceCube.descriptor.imageView = textures.irradianceCube.view;
    textures.irradianceCube.descriptor.sampler = textures.irradianceCube.sampler;
    textures.irradianceCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    textures.irradianceCube.vkDevice = vkDevice;

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Renderpass
    VkRenderPassCreateInfo renderPassCI = Vk::Initializers::RenderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();
    VkRenderPass renderpass;
    VK_CHECK_RESULT(vkCreateRenderPass(vkDevice->logicalDevice, &renderPassCI, nullptr, &renderpass));

    struct {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
        VkFramebuffer framebuffer;
    } offscreen;

    // Offfscreen framebuffer
    {
        // Color attachment
        VkImageCreateInfo imageCreateInfo = Vk::Initializers::ImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent.width = dim;
        imageCreateInfo.extent.height = dim;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK_RESULT(vkCreateImage(vkDevice->logicalDevice, &imageCreateInfo, nullptr, &offscreen.image));

        VkMemoryAllocateInfo memAlloc = Vk::Initializers::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(vkDevice->logicalDevice, offscreen.image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memAlloc, nullptr, &offscreen.memory));
        VK_CHECK_RESULT(vkBindImageMemory(vkDevice->logicalDevice, offscreen.image, offscreen.memory, 0));

        VkImageViewCreateInfo colorImageView = Vk::Initializers::ImageViewCreateInfo();
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.format = format;
        colorImageView.flags = 0;
        colorImageView.subresourceRange = {};
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.image = offscreen.image;
        VK_CHECK_RESULT(vkCreateImageView(vkDevice->logicalDevice, &colorImageView, nullptr, &offscreen.view));

        VkFramebufferCreateInfo fbufCreateInfo = Vk::Initializers::FramebufferCreateInfo();
        fbufCreateInfo.renderPass = renderpass;
        fbufCreateInfo.attachmentCount = 1;
        fbufCreateInfo.pAttachments = &offscreen.view;
        fbufCreateInfo.width = dim;
        fbufCreateInfo.height = dim;
        fbufCreateInfo.layers = 1;
        VK_CHECK_RESULT(vkCreateFramebuffer(vkDevice->logicalDevice, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

        VkCommandBuffer layoutCmd = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        Vk::Utils::SetImageLayout(
            layoutCmd,
            offscreen.image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkDevice->FlushCommandBuffer(layoutCmd, queue, true);
    }

    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        Vk::Initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = Vk::Initializers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vkDevice->logicalDevice, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = { Vk::Initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
    VkDescriptorPoolCreateInfo descriptorPoolCI = Vk::Initializers::DescriptorPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    VK_CHECK_RESULT(vkCreateDescriptorPool(vkDevice->logicalDevice, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = Vk::Initializers::DescriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice, &allocInfo, &descriptorset));
    VkWriteDescriptorSet writeDescriptorSet = Vk::Initializers::WriteDescriptorSet(descriptorset, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &testSkybox->GetTexture2D()->descriptor);
    vkUpdateDescriptorSets(vkDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);

    // Pipeline layout
    struct PushBlock {
        glm::mat4 mvp;
    } pushBlock;

    VkPipelineLayout pipelinelayout;
    std::vector<VkPushConstantRange> pushConstantRanges = {
        Vk::Initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = Vk::Initializers::PipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
    VK_CHECK_RESULT(vkCreatePipelineLayout(vkDevice->logicalDevice, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = Vk::Initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = Vk::Initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = Vk::Initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = Vk::Initializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = Vk::Initializers::PipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = Vk::Initializers::PipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = Vk::Initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = Vk::Initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables);

    Vk::VertexLayout vertexLayout =
    {
        {
            Vk::eComponent::kPosition,
            Vk::eComponent::kNormal,
            Vk::eComponent::kUv,
        }
    };

    // Vertex input state
    VkVertexInputBindingDescription vertexInputBinding = Vk::Initializers::VertexInputBindingDescription(0, vertexLayout.Stride(), VK_VERTEX_INPUT_RATE_VERTEX);
    VkVertexInputAttributeDescription vertexInputAttribute = Vk::Initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

    VkPipelineVertexInputStateCreateInfo vertexInputState = Vk::Initializers::PipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputState.vertexAttributeDescriptionCount = 1;
    vertexInputState.pVertexAttributeDescriptions = &vertexInputAttribute;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = Vk::Initializers::PipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &vertexInputState;
    pipelineCI.renderPass = renderpass;

    shaderStages[0] = LoadShader(GetAssetPath() + "shaders/compiled/filter.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(GetAssetPath() + "shaders/compiled/irradianceFilter.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipeline pipeline;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(vkDevice->logicalDevice, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render
    VkClearValue clearValues[1];
    clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Initializers::RenderPassBeginInfo();
    // Reuse render pass from example pass
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.framebuffer = offscreen.framebuffer;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;

    std::vector<glm::mat4> matrices = {
        // POSITIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // POSITIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        // NEGATIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    VkCommandBuffer cmdBuf = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkViewport viewport = Vk::Initializers::Viewport((float)dim, (float)dim, 0.0f, 1.0f);
    VkRect2D scissor = Vk::Initializers::Rect2D(dim, dim, 0, 0);

    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = numMips;
    subresourceRange.layerCount = 6;

    // Change image layout for all cubemap faces to transfer destination
    Vk::Utils::SetImageLayout(
        cmdBuf,
        textures.irradianceCube.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    for (uint32_t m = 0; m < numMips; ++m) {
        for (uint32_t f = 0; f < 6; ++f) {
            viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
            viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
            vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

            // Render scene from cube face's point of view
            vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Update shader push constant block
            pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

            vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

            VkDeviceSize offsets[1] = { 0 };

            vkCmdBindVertexBuffers(cmdBuf, 0, 1, &testSkybox->vertices.buffer, offsets);
            vkCmdDraw(cmdBuf, testSkybox->vertexCount, 1, 0, 0);

            vkCmdEndRenderPass(cmdBuf);

            Vk::Utils::SetImageLayout(
                cmdBuf,
                offscreen.image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            // Copy region for transfer from framebuffer to cube face
            VkImageCopy copyRegion = {};

            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = { 0, 0, 0 };

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.mipLevel = m;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = { 0, 0, 0 };

            copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
            copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(
                cmdBuf,
                offscreen.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                textures.irradianceCube.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion);

            // Transform framebuffer color attachment back 
            Vk::Utils::SetImageLayout(
                cmdBuf,
                offscreen.image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }

    Vk::Utils::SetImageLayout(
        cmdBuf,
        textures.irradianceCube.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        subresourceRange);

    vkDevice->FlushCommandBuffer(cmdBuf, queue);

    // todo: cleanup
    vkDestroyRenderPass(vkDevice->logicalDevice, renderpass, nullptr);
    vkDestroyFramebuffer(vkDevice->logicalDevice, offscreen.framebuffer, nullptr);
    vkFreeMemory(vkDevice->logicalDevice, offscreen.memory, nullptr);
    vkDestroyImageView(vkDevice->logicalDevice, offscreen.view, nullptr);
    vkDestroyImage(vkDevice->logicalDevice, offscreen.image, nullptr);
    vkDestroyDescriptorPool(vkDevice->logicalDevice, descriptorpool, nullptr);
    vkDestroyDescriptorSetLayout(vkDevice->logicalDevice, descriptorsetlayout, nullptr);
    vkDestroyPipeline(vkDevice->logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(vkDevice->logicalDevice, pipelinelayout, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Generating irradiance cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
}
