#include "Core\EngineImpl.hpp"

#include <include/nfd.h>
#include <stddef.h>

#include <algorithm>
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#include "Core\EngineConfig.hpp"
#include "ECS\Components\CameraComponent.hpp"
#include "ECS\Systems\CameraSystem.hpp"
#include "ECS\Systems\LightSystem.hpp"
#include "Render\Vulkan\Debug.hpp"
#include "Render\Vulkan\Device.hpp"
#include "Render\Vulkan\Exceptions.hpp"
#include "Render\Vulkan\ImGuiImpl.hpp"
#include "Render\Vulkan\Initializers.hpp"
#include "Render\Vulkan\LayoutDescriptor.hpp"
#include "Render\Vulkan\Model.hpp"
#include "Render\Vulkan\SwapChain.hpp"
#include "Render\Vulkan\Texture.hpp"
#include "Render\Vulkan\Texture2D.hpp"
#include "Render\Vulkan\Utils.hpp"
#include "SDL2\SDL_events.h"
#include "SDL2\SDL_messagebox.h"
#include "entt\entity\registry.hpp"
#include "imgui\imgui.h"
#include <random>

using namespace CG;

constexpr uint32_t kIndexRaygen = 0;
constexpr uint32_t kIndexMiss = 1;
constexpr uint32_t kIndexClosestHit = 2;

CG::EngineImpl::EngineImpl(CG::EngineConfig& engineConfig)
    : CG::Engine(engineConfig)
{
    enabledInstanceExtensions.push_back(
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    enabledDeviceExtensions.push_back(
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    enabledDeviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
}

CG::EngineImpl::~EngineImpl() = default;

void CG::EngineImpl::RenderFrame(float deltaTime)
{
    UpdateUniformBuffers();

    PrepareFrame();

    imGui->UpdateUI(deltaTime);

    BuildCommandBuffers();

    // Pipeline stage at which the queue submission will wait (via
    // pWaitSemaphores)
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
    CreateNVRayTracingStoreImage();
    CreateNVRayTracingAccumulationImage();

    SetupSystems();

    PrepareUniformBuffers();

    SetupDescriptorsPool();

    emptyTexture.LoadFromFile(GetAssetPath() + "textures/FFFFFF-1.png", vkDevice,
        queue);
    LoadSkybox(GetAssetPath() + "textures/hdr/Malibu_Overlook_3k.hdr");
    LoadModelAsync(
        "D:/glTF-Sample-Models-master/glTF-Sample-Models-master/2.0/"
        "MetalRoughSpheres/glTF/MetalRoughSpheres.gltf");

    CreateShaderBindingTable(shaderBindingTables.RTX, pipelines.RTX);
    CreateShaderBindingTable(shaderBindingTables.RTX_PBR, pipelines.RTX_PBR);

    BuildCommandBuffers();

    UpdateUniformBuffers();
}

void CG::EngineImpl::Cleanup()
{
    DestroyRTXPipeline();
    DestroyNVRayTracingGeometry();
    DestroyNVRayTracingStoreImage();
    DestroyNVRayTracingAccumulationImage();

    emptyTexture.Destroy();
    cubemapTexture.Destroy();

    testScene = nullptr;
    imGui = nullptr;

    Engine::Cleanup();
}

VkPhysicalDeviceFeatures2 CG::EngineImpl::GetEnabledDeviceFeatures() const
{
    VkPhysicalDeviceFeatures availableFeatures = vkDevice->features;
    VkPhysicalDeviceFeatures enabledFeatures = {};

    if (availableFeatures.fillModeNonSolid) {
        enabledFeatures.fillModeNonSolid = VK_TRUE;
    }

    if (availableFeatures.sampleRateShading) {
        enabledFeatures.sampleRateShading = VK_TRUE;
    }

    // TODO: remove hardcode descr indexing features
    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT
        physicalDeviceDescriptorIndexingFeatures;
    physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    physicalDeviceDescriptorIndexingFeatures.pNext = nullptr;
    physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = true;
    physicalDeviceDescriptorIndexingFeatures
        .shaderStorageBufferArrayNonUniformIndexing
        = true;
    physicalDeviceDescriptorIndexingFeatures
        .shaderUniformBufferArrayNonUniformIndexing
        = true;
    physicalDeviceDescriptorIndexingFeatures
        .shaderSampledImageArrayNonUniformIndexing
        = true;

    VkPhysicalDeviceFeatures2 enabledFeatures2;

    enabledFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabledFeatures2.features = enabledFeatures;
    enabledFeatures2.pNext = &physicalDeviceDescriptorIndexingFeatures;

    return enabledFeatures2;
}

void CG::EngineImpl::CaptureEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_KEYDOWN: {
        if (event.key.keysym.sym == SDLK_f) {
            cameraComponent->position = glm::vec3(0.0f, 0.0f, -cameraComponent->zoom);
            cameraComponent->rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        }
    } break;
    }
}

void CG::EngineImpl::OnWindowResize()
{
    DestroyNVRayTracingAccumulationImage();
    DestroyNVRayTracingStoreImage();
    CreateNVRayTracingStoreImage();
    CreateNVRayTracingAccumulationImage();
}

void CG::EngineImpl::FlushCommandBuffer(VkCommandBuffer commandBuffer)
{
    assert(commandBuffer != VK_NULL_HANDLE);

    vkDevice->FlushCommandBuffer(commandBuffer, queue, true);
}

void CG::EngineImpl::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufInfo.pNext = nullptr;

    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = { uiData.bgColor.r, uiData.bgColor.g, uiData.bgColor.b,
        uiData.bgColor.a };
    // clearValues[1].color = { uiData.bgColor.r, uiData.bgColor.g,
    // uiData.bgColor.b, uiData.bgColor.a };
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

    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
        renderPassBeginInfo.framebuffer = frameBuffers[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

        DrawRayTracingData(i);

        vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo,
            VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

        imGui->DrawFrame(drawCmdBuffers[i]);

        vkCmdEndRenderPass(drawCmdBuffers[i]);

        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void CG::EngineImpl::SetupDescriptorsPool()
{
    constexpr uint32_t typePoolSize = 1024;

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, typePoolSize },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, typePoolSize },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, typePoolSize },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, typePoolSize },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, typePoolSize },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, typePoolSize },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, typePoolSize },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, typePoolSize },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, typePoolSize },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, typePoolSize },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, typePoolSize },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, typePoolSize },
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, typePoolSize },
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI = Vk::Initializers::DescriptorPoolCreateInfo(
        poolSizes, typePoolSize * static_cast<uint32_t>(poolSizes.size()));
    VK_CHECK_RESULT(vkCreateDescriptorPool(
        vkDevice->logicalDevice, &descriptorPoolCI, nullptr, &descriptorPool));
}

void CG::EngineImpl::BindModelMaterials() { }

void CG::EngineImpl::UnbindModelMaterials() { }

void CG::EngineImpl::PrepareUniformBuffers()
{
    auto cameraEntity = registry.create();
    CameraComponent& component = registry.assign<CameraComponent>(cameraEntity);

    component.viewport.height = engineConfig.height;
    component.viewport.width = engineConfig.width;

    uniformBufferVS = &component.uniformBufferVS;

    cameraComponent = &component;

    VK_CHECK_RESULT(
        vkDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &sceneUbo, sizeof(sceneUboData)));

    // Map persistent
    VK_CHECK_RESULT(sceneUbo.Map());

    VK_CHECK_RESULT(
        vkDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &cameraUbo, sizeof(cameraUboData)));

    // Map persistent
    VK_CHECK_RESULT(cameraUbo.Map());
}

void CG::EngineImpl::UpdateUniformBuffers()
{
    glm::vec3 rotation = glm::vec3(75.0f, 40.0f, 0.0f);

    sceneUboData.globalLightDir = glm::vec4(
        sin(glm::radians(rotation.x)) * cos(glm::radians(rotation.y)),
        sin(glm::radians(rotation.y)),
        cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y)), 0.0f);

    sceneUboData.globalLightColor = glm::vec4({ 1.0f, 1.0f, 1.0f, 1.0f }) * 5.0f;
    sceneUboData.projection = cameraComponent->uboVS.projectionMatrix;
    sceneUboData.view = cameraComponent->uboVS.viewMatrix;

    sceneUboData.invProjection = glm::inverse(sceneUboData.projection);
    sceneUboData.invView = glm::inverse(sceneUboData.view);

    float scale = (1.0f / std::max(testScene->GetSize().x, std::max(testScene->GetSize().y, testScene->GetSize().z))) * 0.5f;

    sceneUboData.model = glm::mat4(1.0f);
    sceneUboData.model[0][0] = scale;
    sceneUboData.model[1][1] = scale;
    sceneUboData.model[2][2] = scale;

    sceneUboData.cameraPos = glm::vec4(cameraComponent->position, 1.0f);
    sceneUbo.CopyTo(&sceneUboData, sizeof(sceneUboData));

    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, std::numeric_limits<int>::max());

    cameraUboData.randomSeed = dist(rng);
    cameraUboData.accumulationIndex = cameraComponent->accumulationIndex;
    cameraUbo.CopyTo(&cameraUboData, sizeof(cameraUboData));

    if (!cameraUboData.pauseRendering && !cameraComponent->input.IsMoving()) {
        cameraComponent->accumulationIndex = std::min(cameraComponent->accumulationIndex + 1, std::numeric_limits<int>::max());
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

    if (uiData.isActive) {
        // Init imGui windows and elements
        ImGui::Begin("Coldgaze overlay", &uiData.isActive,
            ImGuiWindowFlags_MenuBar);

        // Edit a color (stored as ~4 floats)
        ImGui::ColorEdit4("Background", &uiData.bgColor.x,
            ImGuiColorEditFlags_NoInputs);

        ImGui::Checkbox("Enable preview quality", &uiData.enablePreviewQuality);

        // Plot some values
        const float dummyData[] = { 0.2f, 0.1f, 1.0f, 0.5f, 0.9f, 2.2f };
        ImGui::PlotLines("Frame Times", dummyData, IM_ARRAYSIZE(dummyData));

        const float oldFov = cameraComponent->fov;
        ImGui::SliderFloat("Camera FOV:", &cameraComponent->fov, 10.0f, 135.0f);

        CameraUboData oldCameraUbo = cameraUboData;

        ImGui::SliderInt("Bounces count:", &cameraUboData.bouncesCount, 1, 64);
        ImGui::SliderInt("Number of samples:", &cameraUboData.numberOfSamples, 1, 64);
        ImGui::SliderFloat("Aperture:", &cameraUboData.aperture, 0.0f, 1.0f);
        ImGui::SliderFloat("Focus distance:", &cameraUboData.focusDistance, 0.0f, 64.0f);

        if (cameraUboData.pauseRendering) {
            cameraUboData.pauseRendering = !ImGui::Button("Resume");
        } else {
            cameraUboData.pauseRendering = ImGui::Button("Pause");
        }

        if (std::tie(oldCameraUbo.aperture, oldCameraUbo.bouncesCount, oldCameraUbo.focusDistance, oldCameraUbo.numberOfSamples) != std::tie(cameraUboData.aperture, cameraUboData.bouncesCount, cameraUboData.focusDistance, cameraUboData.numberOfSamples)
            || oldFov != cameraComponent->fov) {
            cameraComponent->ResetSamples();
        }

        ImGui::End();
    }

    {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("File menu", nullptr, false, false);
                {
                    if (ImGui::MenuItem("Open scene", "*.gltf")) {
                        nfdchar_t* outPath = nullptr;
#pragma warning(push)
#pragma warning(disable : 26812)
                        nfdresult_t result = NFD_OpenDialog(nullptr, nullptr, &outPath);
#pragma warning(pop)
                        if (result == NFD_OKAY) {
                            LoadModelAsync(outPath);
                            free(outPath);
                        } else if (result == NFD_ERROR) {
                            std::cerr << NFD_GetError() << std::endl;
                        }
                    }

                    if (ImGui::MenuItem("Set skybox", "*.hdr")) {
                        nfdchar_t* outPath = nullptr;
#pragma warning(push)
#pragma warning(disable : 26812)
                        nfdresult_t result = NFD_OpenDialog(nullptr, nullptr, &outPath);
#pragma warning(pop)
                        if (result == NFD_OKAY) {
                            LoadSkybox(outPath);
                            free(outPath);
                        } else if (result == NFD_ERROR) {
                            std::cerr << NFD_GetError() << std::endl;
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            if (ImGui::BeginMenu("Overlay")) {
                if (ImGui::MenuItem("Show", nullptr, false, !uiData.isActive)) {
                    uiData.isActive = true;
                }
                if (ImGui::MenuItem("Hide", nullptr, false, uiData.isActive)) {
                    uiData.isActive = false;
                }
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
        if (testScene) {
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
    try {
        if (testScene) {
            DestroyNVRayTracingGeometry();
            DestroyRTXPipeline();
            // SetupRTXEnviromentDescriptorSet();
        }

        LoadModel(modelFilePath);

        testScene->SetLoaded(true);

        UpdateUniformBuffers();
        CreateNVRayTracingGeometry();
        SetupRTXModelDescriptorSets();

        CreateRTXPipeline();
    } catch (const Vk::AssetLoadingException& e) {
        std::cerr << e.what() << std::endl;

        const SDL_MessageBoxButtonData buttons[] = {
            { /* .flags, .buttonid, .text */ 0, 0, "ok" },
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
            throw std::runtime_error(
                "Error while trying to display SDL message box:" + std::string(SDL_GetError()));
        }
    }
}

void CG::EngineImpl::LoadSkybox(const std::string& cubeMapFilePath)
{
    try {
        cubemapTexture.LoadFromFile(cubeMapFilePath, vkDevice, queue, true);
        SetupRTXEnviromentDescriptorSet();
    } catch (const Vk::AssetLoadingException& e) {
        std::cerr << e.what() << std::endl;

        const SDL_MessageBoxButtonData buttons[] = {
            { /* .flags, .buttonid, .text */ 0, 0, "ok" },
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
            throw std::runtime_error(
                "Error while trying to display SDL message box:" + std::string(SDL_GetError()));
        }
    }
}

void CG::EngineImpl::CreateBottomLevelAccelerationStructure(
    const VkGeometryNV* geometries,
    uint32_t blasIndex,
    size_t geomCount)
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
        accelerationStructureInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;

        VkAccelerationStructureCreateInfoNV accelerationStructureCI = {};
        accelerationStructureCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
        accelerationStructureCI.info = accelerationStructureInfo;
        VK_CHECK_RESULT(vkCreateAccelerationStructureNV(
            vkDevice->logicalDevice, &accelerationStructureCI, nullptr,
            &bottomLevelAS.accelerationStructure));

        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.accelerationStructure = bottomLevelAS.accelerationStructure;

        VkMemoryRequirements2 memoryRequirements2 = {};
        vkGetAccelerationStructureMemoryRequirementsNV(
            vkDevice->logicalDevice, &memoryRequirementsInfo, &memoryRequirements2);

        VkMemoryAllocateInfo memoryAllocateInfo = Vk::Initializers::MemoryAllocateInfo();
        memoryAllocateInfo.allocationSize = memoryRequirements2.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(
            memoryRequirements2.memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice,
            &memoryAllocateInfo, nullptr,
            &bottomLevelAS.memory));

        VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo = {};
        accelerationStructureMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
        accelerationStructureMemoryInfo.accelerationStructure = bottomLevelAS.accelerationStructure;
        accelerationStructureMemoryInfo.memory = bottomLevelAS.memory;
        VK_CHECK_RESULT(vkBindAccelerationStructureMemoryNV(
            vkDevice->logicalDevice, 1, &accelerationStructureMemoryInfo));

        VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(
            vkDevice->logicalDevice, bottomLevelAS.accelerationStructure,
            sizeof(uint64_t), &bottomLevelAS.handle));
    }

    // Build step
    {
        // Acceleration structure build requires some scratch space to store
        // temporary information
        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

        VkMemoryRequirements2 memReqBottomLevelAS;
        memoryRequirementsInfo.accelerationStructure = bottomLevelAS.accelerationStructure;
        vkGetAccelerationStructureMemoryRequirementsNV(
            vkDevice->logicalDevice, &memoryRequirementsInfo, &memReqBottomLevelAS);

        const VkDeviceSize scratchBufferSize = memReqBottomLevelAS.memoryRequirements.size;

        Vk::Buffer scratchBuffer;
        VK_CHECK_RESULT(vkDevice->CreateBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &scratchBuffer, scratchBufferSize));

        VkCommandBuffer cmdBuffer = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkAccelerationStructureInfoNV buildInfo {};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
        buildInfo.geometryCount = static_cast<uint32_t>(geomCount);
        buildInfo.pGeometries = geometries;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;

        vkCmdBuildAccelerationStructureNV(cmdBuffer, &buildInfo, VK_NULL_HANDLE, 0,
            VK_FALSE,
            bottomLevelAS.accelerationStructure,
            VK_NULL_HANDLE, scratchBuffer.buffer, 0);

        VkMemoryBarrier memoryBarrier = Vk::Initializers::CreateMemoryBarrier();
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
            0, 1, &memoryBarrier, 0, 0, 0, 0);

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
        VK_CHECK_RESULT(vkCreateAccelerationStructureNV(
            vkDevice->logicalDevice, &accelerationStructureCI, nullptr,
            &topLevelAS.accelerationStructure));

        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.accelerationStructure = topLevelAS.accelerationStructure;

        VkMemoryRequirements2 memoryRequirements2 = {};
        vkGetAccelerationStructureMemoryRequirementsNV(
            vkDevice->logicalDevice, &memoryRequirementsInfo, &memoryRequirements2);

        VkMemoryAllocateInfo memoryAllocateInfo = Vk::Initializers::MemoryAllocateInfo();
        memoryAllocateInfo.allocationSize = memoryRequirements2.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(
            memoryRequirements2.memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice,
            &memoryAllocateInfo, nullptr,
            &topLevelAS.memory));

        VkBindAccelerationStructureMemoryInfoNV accelerationStructureMemoryInfo = {};
        accelerationStructureMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
        accelerationStructureMemoryInfo.accelerationStructure = topLevelAS.accelerationStructure;
        accelerationStructureMemoryInfo.memory = topLevelAS.memory;
        VK_CHECK_RESULT(vkBindAccelerationStructureMemoryNV(
            vkDevice->logicalDevice, 1, &accelerationStructureMemoryInfo));

        VK_CHECK_RESULT(vkGetAccelerationStructureHandleNV(
            vkDevice->logicalDevice, topLevelAS.accelerationStructure,
            sizeof(uint64_t), &topLevelAS.handle));
    }

    {
        // Single instance with a 3x4 transform matrix for the ray traced triangle
        Vk::Buffer instanceBuffer;

        std::vector<GeometryInstance> geometryInstances(blasData.size());
        for (size_t i = 0; i < geometryInstances.size(); ++i) {
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
            &instanceBuffer, sizeof(GeometryInstance) * geometryInstances.size(),
            geometryInstances.data()));

        // Acceleration structure build requires some scratch space to store
        // temporary information
        VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo {};
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
        memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

        VkMemoryRequirements2 memReqTopLevelAS;
        memoryRequirementsInfo.accelerationStructure = topLevelAS.accelerationStructure;
        vkGetAccelerationStructureMemoryRequirementsNV(
            vkDevice->logicalDevice, &memoryRequirementsInfo, &memReqTopLevelAS);

        const VkDeviceSize scratchBufferSize = memReqTopLevelAS.memoryRequirements.size;

        Vk::Buffer scratchBuffer;
        VK_CHECK_RESULT(vkDevice->CreateBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &scratchBuffer, scratchBufferSize));

        VkCommandBuffer cmdBuffer = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkAccelerationStructureInfoNV buildInfo {};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
        buildInfo.pGeometries = 0;
        buildInfo.geometryCount = 0;
        buildInfo.instanceCount = static_cast<uint32_t>(blasData.size());

        vkCmdBuildAccelerationStructureNV(cmdBuffer, &buildInfo,
            instanceBuffer.buffer, 0, VK_FALSE,
            topLevelAS.accelerationStructure,
            VK_NULL_HANDLE, scratchBuffer.buffer, 0);

        VkMemoryBarrier memoryBarrier = Vk::Initializers::CreateMemoryBarrier();
        memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
            0, 1, &memoryBarrier, 0, 0, 0, 0);

        vkDevice->FlushCommandBuffer(cmdBuffer, queue);

        instanceBuffer.Destroy();
    }
}

void CG::EngineImpl::LoadNVRayTracingProcs()
{
    VkDevice device = vkDevice->logicalDevice;

    // Get VK_NV_ray_tracing related function pointers
    vkCreateAccelerationStructureNV = reinterpret_cast<PFN_vkCreateAccelerationStructureNV>(
        vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureNV"));
    vkDestroyAccelerationStructureNV = reinterpret_cast<PFN_vkDestroyAccelerationStructureNV>(
        vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureNV"));
    vkBindAccelerationStructureMemoryNV = reinterpret_cast<PFN_vkBindAccelerationStructureMemoryNV>(
        vkGetDeviceProcAddr(device, "vkBindAccelerationStructureMemoryNV"));
    vkGetAccelerationStructureHandleNV = reinterpret_cast<PFN_vkGetAccelerationStructureHandleNV>(
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureHandleNV"));
    vkGetAccelerationStructureMemoryRequirementsNV = reinterpret_cast<PFN_vkGetAccelerationStructureMemoryRequirementsNV>(
        vkGetDeviceProcAddr(
            device, "vkGetAccelerationStructureMemoryRequirementsNV"));
    vkCmdBuildAccelerationStructureNV = reinterpret_cast<PFN_vkCmdBuildAccelerationStructureNV>(
        vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructureNV"));
    vkCreateRayTracingPipelinesNV = reinterpret_cast<PFN_vkCreateRayTracingPipelinesNV>(
        vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesNV"));
    vkGetRayTracingShaderGroupHandlesNV = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesNV>(
        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesNV"));
    vkCmdTraceRaysNV = reinterpret_cast<PFN_vkCmdTraceRaysNV>(
        vkGetDeviceProcAddr(device, "vkCmdTraceRaysNV"));
}

void CG::EngineImpl::CreateNVRayTracingGeometry()
{
    assert(testScene);

    blasData.resize(testScene->GetPrimitivesCount());
    std::vector<VkGeometryNV> geometries(testScene->GetPrimitivesCount());
    uint32_t currentGeomIndex = 0;

    for (const auto& node : testScene->GetFlatNodes()) {
        if (node->mesh) {
            for (const auto& primitive : node->mesh->primitives) {
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

                blasData[currentGeomIndex].transform = sceneUboData.model * node->GetWorldMatrix();
                CreateBottomLevelAccelerationStructure(&geometry, currentGeomIndex, 1);

                ++currentGeomIndex;
            }
        }
    }

    CreateTopLevelAccelerationStructure();
}

void CG::EngineImpl::DestroyNVRayTracingGeometry()
{
    for (const auto& blas : blasData) {
        vkFreeMemory(vkDevice->logicalDevice, blas.memory, nullptr);
    }

    vkFreeMemory(vkDevice->logicalDevice, topLevelAS.memory, nullptr);

    for (const auto& blas : blasData) {
        vkDestroyAccelerationStructureNV(vkDevice->logicalDevice,
            blas.accelerationStructure, nullptr);
    }

    vkDestroyAccelerationStructureNV(vkDevice->logicalDevice,
        topLevelAS.accelerationStructure, nullptr);
}

void CG::EngineImpl::CreateNVRayTracingStoreImage()
{
    VkImageCreateInfo image = Vk::Initializers::ImageCreateInfo();
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = vkSwapChain->colorFormat;
    image.extent.width = engineConfig.width;
    image.extent.height = engineConfig.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(vkDevice->logicalDevice, &image, nullptr,
        &storageImage.image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(vkDevice->logicalDevice, storageImage.image,
        &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo = Vk::Initializers::MemoryAllocateInfo();
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memoryAllocateInfo,
        nullptr, &storageImage.memory));
    VK_CHECK_RESULT(vkBindImageMemory(vkDevice->logicalDevice, storageImage.image,
        storageImage.memory, 0));

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
    VK_CHECK_RESULT(vkCreateImageView(vkDevice->logicalDevice, &colorImageView,
        nullptr, &storageImage.view));

    VkCommandBuffer cmdBuffer = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Vk::Utils::SetImageLayout(cmdBuffer, storageImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    vkDevice->FlushCommandBuffer(cmdBuffer, queue);
}

void CG::EngineImpl::DestroyNVRayTracingStoreImage()
{
    vkDestroyImageView(vkDevice->logicalDevice, storageImage.view, nullptr);
    vkDestroyImage(vkDevice->logicalDevice, storageImage.image, nullptr);
    vkFreeMemory(vkDevice->logicalDevice, storageImage.memory, nullptr);
}

void CG::EngineImpl::CreateNVRayTracingAccumulationImage()
{
    VkImageCreateInfo image = Vk::Initializers::ImageCreateInfo();
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = vkSwapChain->colorFormat;
    image.extent.width = engineConfig.width;
    image.extent.height = engineConfig.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK_RESULT(vkCreateImage(vkDevice->logicalDevice, &image, nullptr,
        &accumulationImage.image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(vkDevice->logicalDevice, accumulationImage.image,
        &memReqs);
    VkMemoryAllocateInfo memoryAllocateInfo = Vk::Initializers::MemoryAllocateInfo();
    memoryAllocateInfo.allocationSize = memReqs.size;
    memoryAllocateInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memoryAllocateInfo,
        nullptr, &accumulationImage.memory));
    VK_CHECK_RESULT(vkBindImageMemory(vkDevice->logicalDevice, accumulationImage.image,
        accumulationImage.memory, 0));

    VkImageViewCreateInfo colorImageView = Vk::Initializers::ImageViewCreateInfo();
    colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorImageView.format = vkSwapChain->colorFormat;
    colorImageView.subresourceRange = {};
    colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorImageView.subresourceRange.baseMipLevel = 0;
    colorImageView.subresourceRange.levelCount = 1;
    colorImageView.subresourceRange.baseArrayLayer = 0;
    colorImageView.subresourceRange.layerCount = 1;
    colorImageView.image = accumulationImage.image;
    VK_CHECK_RESULT(vkCreateImageView(vkDevice->logicalDevice, &colorImageView,
        nullptr, &accumulationImage.view));

    VkCommandBuffer cmdBuffer = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Vk::Utils::SetImageLayout(cmdBuffer, accumulationImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    vkDevice->FlushCommandBuffer(cmdBuffer, queue);
}

void CG::EngineImpl::DestroyNVRayTracingAccumulationImage()
{
    vkDestroyImageView(vkDevice->logicalDevice, accumulationImage.view, nullptr);
    vkDestroyImage(vkDevice->logicalDevice, accumulationImage.image, nullptr);
    vkFreeMemory(vkDevice->logicalDevice, accumulationImage.memory, nullptr);
}

void CG::EngineImpl::CreateShaderBindingTable(Vk::Buffer& shaderBindingTable, VkPipeline pipeline)
{
    const uint32_t sbtSize = rayTracingProperties.shaderGroupHandleSize * 3;
    VK_CHECK_RESULT(vkDevice->CreateBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &shaderBindingTable, sbtSize));
    shaderBindingTable.Map();

    auto shaderHandleStorage = new uint8_t[sbtSize];
    // Get shader identifiers
    VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesNV(
        vkDevice->logicalDevice, pipeline, 0, 3, sbtSize,
        shaderHandleStorage));
    auto* data = static_cast<uint8_t*>(shaderBindingTable.mapped);

    // Copy the shader identifiers to the shader binding table
    data += CopyShaderIdentifier(data, shaderHandleStorage, kIndexRaygen);
    data += CopyShaderIdentifier(data, shaderHandleStorage, kIndexMiss);
    data += CopyShaderIdentifier(data, shaderHandleStorage, kIndexClosestHit);
    shaderBindingTable.Unmap();
}

VkDeviceSize CG::EngineImpl::CopyShaderIdentifier(
    uint8_t* data,
    const uint8_t* shaderHandleStorage,
    uint32_t groupIndex)
{
    const uint32_t shaderGroupHandleSize = rayTracingProperties.shaderGroupHandleSize;
    memcpy(data, shaderHandleStorage + groupIndex * shaderGroupHandleSize,
        shaderGroupHandleSize);
    return shaderGroupHandleSize;
}

void CG::EngineImpl::CreateRTXPipeline()
{
    const std::vector<VkDescriptorSetLayout> setLayouts = {
        descriptorSetLayouts.rtxRaygenLayout.layout,
        descriptorSetLayouts.rtxRayhitLayout.layout,
        descriptorSetLayouts.rtxRaymissLayout.layout,
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = Vk::Initializers::PipelineLayoutCreateInfo(setLayouts);

    VK_CHECK_RESULT(vkCreatePipelineLayout(vkDevice->logicalDevice,
        &pipelineLayoutCreateInfo, nullptr,
        &pipelineLayouts.rtxPipelineLayout));

    const uint32_t shaderIndexRaygen = 0;
    const uint32_t shaderIndexMiss = 1;
    const uint32_t shaderIndexClosestHit = 2;

    std::array<VkPipelineShaderStageCreateInfo, 3> rtxShaderStages;
    rtxShaderStages[shaderIndexRaygen] = LoadShader(GetAssetPath() + "shaders/compiled/raygen.rgen.spv",
        VK_SHADER_STAGE_RAYGEN_BIT_NV);
    rtxShaderStages[shaderIndexMiss] = LoadShader(GetAssetPath() + "shaders/compiled/miss.rmiss.spv",
        VK_SHADER_STAGE_MISS_BIT_NV);
    rtxShaderStages[shaderIndexClosestHit] = LoadShader(GetAssetPath() + "shaders/compiled/closesthit.rchit.spv",
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

    std::array<VkRayTracingShaderGroupCreateInfoNV, 3> groups {};
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

    VkRayTracingPipelineCreateInfoNV rayPipelineInfo {};
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(rtxShaderStages.size());
    rayPipelineInfo.pStages = rtxShaderStages.data();
    rayPipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    rayPipelineInfo.pGroups = groups.data();
    rayPipelineInfo.maxRecursionDepth = 1;
    rayPipelineInfo.layout = pipelineLayouts.rtxPipelineLayout;

    VK_CHECK_RESULT(
        vkCreateRayTracingPipelinesNV(vkDevice->logicalDevice, VK_NULL_HANDLE, 1,
            &rayPipelineInfo, nullptr, &pipelines.RTX));

    std::array<VkPipelineShaderStageCreateInfo, 3> pbrShaderStages;
    pbrShaderStages[shaderIndexRaygen] = LoadShader(GetAssetPath() + "shaders/compiled/raygen_PBR.rgen.spv",
        VK_SHADER_STAGE_RAYGEN_BIT_NV);
    pbrShaderStages[shaderIndexMiss] = LoadShader(GetAssetPath() + "shaders/compiled/miss_PBR.rmiss.spv",
        VK_SHADER_STAGE_MISS_BIT_NV);
    pbrShaderStages[shaderIndexClosestHit] = LoadShader(GetAssetPath() + "shaders/compiled/closesthit_PBR.rchit.spv",
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

    rayPipelineInfo.stageCount = static_cast<uint32_t>(pbrShaderStages.size());
    rayPipelineInfo.pStages = pbrShaderStages.data();

    VK_CHECK_RESULT(
        vkCreateRayTracingPipelinesNV(vkDevice->logicalDevice, VK_NULL_HANDLE, 1,
            &rayPipelineInfo, nullptr, &pipelines.RTX_PBR));
}

void CG::EngineImpl::DestroyRTXPipeline()
{
    vkDestroyPipeline(vkDevice->logicalDevice, pipelines.RTX_PBR, nullptr);
    vkDestroyPipeline(vkDevice->logicalDevice, pipelines.RTX, nullptr);
    vkDestroyPipelineLayout(vkDevice->logicalDevice,
        pipelineLayouts.rtxPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(vkDevice->logicalDevice,
        descriptorSetLayouts.rtxRaygenLayout.layout,
        nullptr);
    vkDestroyDescriptorSetLayout(vkDevice->logicalDevice,
        descriptorSetLayouts.rtxRayhitLayout.layout,
        nullptr);
    // vkDestroyDescriptorSetLayout(vkDevice->logicalDevice,
    // descriptorSetLayouts.rtxRaymissLayout.layout, nullptr);
}

void CG::EngineImpl::CreateRTXPipelineLayout() { }

void CG::EngineImpl::DestroyRTXPipelineLayout() { }

void CG::EngineImpl::SetupRTXModelDescriptorSets()
{
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings({
            { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1,
                VK_SHADER_STAGE_RAYGEN_BIT_NV },
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV },
            { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV },
            { 3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV },
            { 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV },
        });

        VkDescriptorSetLayoutCreateInfo layoutInfo {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        layoutInfo.pBindings = setLayoutBindings.data();
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
            vkDevice->logicalDevice, &layoutInfo, nullptr,
            &descriptorSetLayouts.rtxRaygenLayout.layout));

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = Vk::Initializers::DescriptorSetAllocateInfo(
            descriptorPool, &descriptorSetLayouts.rtxRaygenLayout.layout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice,
            &descriptorSetAllocateInfo,
            &descriptorSets.rtxRaygen));

        descriptorSetLayouts.rtxRaygenLayout.created = true;

        VkWriteDescriptorSetAccelerationStructureNV
            descriptorAccelerationStructureInfo {};
        descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
        descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
        descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.accelerationStructure;

        VkWriteDescriptorSet accelerationStructureWrite {};
        accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        // The specialized acceleration structure descriptor has to be chained
        accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
        accelerationStructureWrite.dstSet = descriptorSets.rtxRaygen;
        accelerationStructureWrite.dstBinding = 0;
        accelerationStructureWrite.descriptorCount = 1;
        accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

        VkDescriptorImageInfo storageImageDescriptor {};
        storageImageDescriptor.imageView = storageImage.view;
        storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo accumulationImageDescriptor {};
        accumulationImageDescriptor.imageView = accumulationImage.view;
        accumulationImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            accelerationStructureWrite,
            Vk::Initializers::WriteDescriptorSet(descriptorSets.rtxRaygen,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                1, &storageImageDescriptor),
            Vk::Initializers::WriteDescriptorSet(descriptorSets.rtxRaygen,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                2, &sceneUbo.descriptor),
            Vk::Initializers::WriteDescriptorSet(descriptorSets.rtxRaygen,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                3, &accumulationImageDescriptor),
            Vk::Initializers::WriteDescriptorSet(descriptorSets.rtxRaygen,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                4, &cameraUbo.descriptor),
        };
        vkUpdateDescriptorSets(vkDevice->logicalDevice,
            static_cast<uint32_t>(writeDescriptorSets.size()),
            writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
    }

    {
        uint32_t primCount = static_cast<uint32_t>(testScene->GetPrimitivesCount());

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, nullptr }, // vertexBuffers[]
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, nullptr }, // indexBuffers[]
            { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, nullptr }, // materials[]
            { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, nullptr }, // baseColorTextures[]
            { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
                nullptr }, // physicalDescriptorTextures[]
            { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, nullptr }, // normalTextures[]
            { 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
                nullptr }, // ambientOcclusionTextures[]
            { 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, primCount,
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, nullptr }, // emissiveTextures[]
        };
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
        descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
        descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
            vkDevice->logicalDevice, &descriptorSetLayoutCI, nullptr,
            &descriptorSetLayouts.rtxRayhitLayout.layout));

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = Vk::Initializers::DescriptorSetAllocateInfo(
            descriptorPool, &descriptorSetLayouts.rtxRayhitLayout.layout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice,
            &descriptorSetAllocateInfo,
            &descriptorSets.rtxRayhit));

        descriptorSetLayouts.rtxRayhitLayout.created = true;

        std::vector<VkDescriptorBufferInfo> dbiVert;
        std::vector<VkDescriptorBufferInfo> dbiIdx;
        std::vector<VkDescriptorBufferInfo> dbiMaterial;
        std::vector<VkDescriptorImageInfo> diiBaseCol;
        std::vector<VkDescriptorImageInfo> diiPhysicalDescr;
        std::vector<VkDescriptorImageInfo> diiNormal;
        std::vector<VkDescriptorImageInfo> diiOcclusion;
        std::vector<VkDescriptorImageInfo> diiEmissive;

        for (const auto& node : testScene->GetFlatNodes()) {
            if (node->mesh) {
                for (const auto& primitive : node->mesh->primitives) {
                    dbiVert.push_back(primitive->vertices.descriptor);
                    dbiIdx.push_back(primitive->indices.descriptor);
                    dbiMaterial.push_back(primitive->material.materialParams.descriptor);
                    diiBaseCol.push_back(
                        primitive->material.baseColorTexture
                            ? primitive->material.baseColorTexture->texture.descriptor
                            : emptyTexture.descriptor);
                    diiPhysicalDescr.push_back(
                        primitive->material.metallicRoughnessTexture
                            ? primitive->material.metallicRoughnessTexture->texture
                                  .descriptor
                            : emptyTexture.descriptor);
                    diiNormal.push_back(
                        primitive->material.normalTexture
                            ? primitive->material.normalTexture->texture.descriptor
                            : emptyTexture.descriptor);
                    diiOcclusion.push_back(
                        primitive->material.occlusionTexture
                            ? primitive->material.occlusionTexture->texture.descriptor
                            : emptyTexture.descriptor);
                    diiEmissive.push_back(
                        primitive->material.emissiveTexture
                            ? primitive->material.emissiveTexture->texture.descriptor
                            : emptyTexture.descriptor);
                }
            }
        }

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0,
                dbiVert.data(), static_cast<uint32_t>(dbiVert.size())),
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                dbiIdx.data(), static_cast<uint32_t>(dbiIdx.size())),
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2,
                dbiMaterial.data(), static_cast<uint32_t>(dbiMaterial.size())),
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                3, diiBaseCol.data(), static_cast<uint32_t>(diiBaseCol.size())),
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                4, diiPhysicalDescr.data(),
                static_cast<uint32_t>(diiPhysicalDescr.size())),
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                5, diiNormal.data(), static_cast<uint32_t>(diiNormal.size())),
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                6, diiOcclusion.data(), static_cast<uint32_t>(diiOcclusion.size())),
            Vk::Initializers::WriteDescriptorSet(
                descriptorSets.rtxRayhit, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                7, diiEmissive.data(), static_cast<uint32_t>(diiEmissive.size())),
        };
        vkUpdateDescriptorSets(vkDevice->logicalDevice,
            static_cast<uint32_t>(writeDescriptorSets.size()),
            writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
    }
}

void CG::EngineImpl::SetupRTXEnviromentDescriptorSet()
{
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
            VK_SHADER_STAGE_MISS_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
            nullptr }, // equirectangularMap
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.pBindings = setLayoutBindings.data();
    descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
        vkDevice->logicalDevice, &descriptorSetLayoutCI, nullptr,
        &descriptorSetLayouts.rtxRaymissLayout.layout));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = Vk::Initializers::DescriptorSetAllocateInfo(
        descriptorPool, &descriptorSetLayouts.rtxRaymissLayout.layout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(vkDevice->logicalDevice,
        &descriptorSetAllocateInfo,
        &descriptorSets.rtxRaymiss));

    descriptorSetLayouts.rtxRaymissLayout.created = true;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        Vk::Initializers::WriteDescriptorSet(
            descriptorSets.rtxRaymiss, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            0, &cubemapTexture.descriptor, 1),
    };
    vkUpdateDescriptorSets(vkDevice->logicalDevice,
        static_cast<uint32_t>(writeDescriptorSets.size()),
        writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void CG::EngineImpl::DrawRayTracingData(uint32_t swapChainImageIndex)
{
    const VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1, 0, 1 };

    const std::vector<VkDescriptorSet> descriptorsets = {
        descriptorSets.rtxRaygen,
        descriptorSets.rtxRayhit,
        descriptorSets.rtxRaymiss,
    };

    VkPipeline pipeline = uiData.enablePreviewQuality ? pipelines.RTX_PBR : pipelines.RTX;
    Vk::Buffer& shaderBindingTable = uiData.enablePreviewQuality ? shaderBindingTables.RTX_PBR : shaderBindingTables.RTX;

    vkCmdBindPipeline(drawCmdBuffers[swapChainImageIndex],
        VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline);

    vkCmdBindDescriptorSets(drawCmdBuffers[swapChainImageIndex],
        VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
        pipelineLayouts.rtxPipelineLayout, 0,
        static_cast<uint32_t>(descriptorsets.size()),
        descriptorsets.data(), 0, 0);

    VkDeviceSize bindingOffsetRayGenShader = rayTracingProperties.shaderGroupHandleSize * kIndexRaygen;
    VkDeviceSize bindingOffsetMissShader = rayTracingProperties.shaderGroupHandleSize * kIndexMiss;
    VkDeviceSize bindingOffsetHitShader = rayTracingProperties.shaderGroupHandleSize * kIndexClosestHit;
    VkDeviceSize bindingStride = rayTracingProperties.shaderGroupHandleSize;

    vkCmdTraceRaysNV(drawCmdBuffers[swapChainImageIndex],
        shaderBindingTable.buffer, bindingOffsetRayGenShader,
        shaderBindingTable.buffer, bindingOffsetMissShader,
        bindingStride, shaderBindingTable.buffer,
        bindingOffsetHitShader, bindingStride, VK_NULL_HANDLE, 0, 0,
        engineConfig.width, engineConfig.height, 1);

    // Prepare current swapchain image as transfer destination
    Vk::Utils::SetImageLayout(
        drawCmdBuffers[swapChainImageIndex],
        vkSwapChain->images[swapChainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    // Prepare ray tracing output image as transfer source
    Vk::Utils::SetImageLayout(drawCmdBuffers[swapChainImageIndex],
        storageImage.image, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        subresourceRange);

    VkImageCopy copyRegion {};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.srcOffset = { 0, 0, 0 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstOffset = { 0, 0, 0 };
    copyRegion.extent = { engineConfig.width, engineConfig.height, 1 };
    vkCmdCopyImage(drawCmdBuffers[swapChainImageIndex], storageImage.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        vkSwapChain->images[swapChainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // Transition swap chain image back for presentation
    Vk::Utils::SetImageLayout(drawCmdBuffers[swapChainImageIndex],
        vkSwapChain->images[swapChainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        subresourceRange);

    // Transition ray tracing output image back to general layout
    Vk::Utils::SetImageLayout(drawCmdBuffers[swapChainImageIndex],
        storageImage.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL, subresourceRange);
}
