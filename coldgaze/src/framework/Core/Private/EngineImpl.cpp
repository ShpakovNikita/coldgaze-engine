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

	LoadSkybox(GetAssetPath() + "textures/hdr/Malibu_Overlook_3k.hdr");
	LoadModelAsync(GetAssetPath() + "models/FlightHelmet/glTF/FlightHelmet.gltf");

	PreparePipelines();
	BuildCommandBuffers();
}

void CG::EngineImpl::Cleanup()
{
	testSkybox = nullptr;
	testScene = nullptr;
	imGui = nullptr;

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
    const std::vector<Vk::GLTFModel::Texture>& textures = testScene->GetTextures();

    for (auto& material : testScene->GetMaterials())
    {
        auto& baseColorImage = images[textures[material.baseColorTextureIndex].imageIndex];
        auto& normalMapImage = images[textures[material.normalMapTextureIndex].imageIndex];
        auto& metallicRoughnessImage = images[textures[material.metallicRoughnessTextureIndex].imageIndex];

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
