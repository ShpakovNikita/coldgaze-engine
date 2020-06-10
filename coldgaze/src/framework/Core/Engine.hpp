#pragma once
#include "Render/Vulkan/Buffer.hpp"
#include "SDL2/SDL_events.h"
#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_core.h"
#include <chrono>
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

struct SDL_Window;
struct CameraComponent;
class ICGSystem;
union SDL_Event;

namespace CG {
// TODO: get rid of vertex here
struct Vertex {
    float position[3];
    float color[3];
};

namespace Vk {
    class SwapChain;
    class Device;
    class ImGuiImpl;
}
struct EngineConfig;
class InputHandler;
class Window;

class Engine {
public:
    Engine(CG::EngineConfig& engineConfig);
    virtual ~Engine();

    void Run();

    entt::registry& GetRegistry();
    const Vk::Device* GetDevice() const;

    const InputHandler* GetInputHandler() const;
    const Window* GetCurrentWindow() const;
    const uint32_t GetSampleCount() const;

    // TODO: move to some shader manager
    VkPipelineShaderStageCreateInfo LoadShader(const std::string& filename, VkShaderStageFlagBits stage);
    const std::string GetAssetPath() const;

protected:
    virtual VkPhysicalDeviceFeatures2 GetEnabledDeviceFeatures() const;
    virtual void RenderFrame(float deltaTime);
    virtual void Prepare();
    virtual void Cleanup();

    virtual void OnWindowResize() {};

    virtual void BuildCommandBuffers();

    virtual void CaptureEvent(const SDL_Event&) {};

    // Frame utils
    void PrepareFrame();
    void SubmitFrame();

    CG::EngineConfig& engineConfig;

    SDL_Window* window = nullptr;
    Vk::Device* vkDevice = nullptr;
    Vk::SwapChain* vkSwapChain = nullptr;
    VkQueue queue = {};

    VkPhysicalDevice vkPhysicalDevice = {};
    VkInstance vkInstance = {};
    VkSurfaceKHR surface = {};
    VkCommandPool vkCmdPool = {};
    std::vector<VkCommandBuffer> drawCmdBuffers;
    std::vector<VkFence> waitFences;
    VkRenderPass renderPass = {};
    VkPipelineCache pipelineCache = {};
    std::vector<VkFramebuffer> frameBuffers;
    VkSubmitInfo submitInfo = {};

    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    struct {
        // Swap chain image presentation
        VkSemaphore presentComplete;
        // Command buffer submission and execution
        VkSemaphore renderComplete;
    } semaphores = {};

    struct
    {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } depthStencil = {};

    // TODO: move inside scene class
    entt::registry registry;
    std::vector<std::unique_ptr<ICGSystem>> systems;

    // active frame buffer index
    uint32_t currentBuffer = 0;

    std::unique_ptr<Vk::ImGuiImpl> imGui;

    std::unique_ptr<InputHandler> inputHandler;
    std::unique_ptr<Window> currentWindow;

    void InitRayTracing();
    VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties;

    CameraComponent* cameraComponent = nullptr;

    std::vector<const char*> enabledInstanceExtensions;
    std::vector<const char*> enabledDeviceExtensions;

private:
    struct {
        struct {
            VkImage image;
            VkImageView view;
            VkDeviceMemory memory;
        } color;
        struct {
            VkImage image;
            VkImageView view;
            VkDeviceMemory memory;
        } depth;
    } multisampleTarget;

    bool Init();
    void MainLoop(float deltaTime);

    void HandleSystemInput(const SDL_Event& event);

    // Init steps
    bool InitSDL();
    bool InitWindow();
    bool InitGraphicsAPI();

    // Graphics API steps
    bool CreateVkInstance();
    bool SetupDebugging();
    bool CreateDevices();
    bool CreateSwapChain();
    bool SetupSemaphores();

    // Prepare steps
    void InitSwapChain();
    void CreateCommandPool();
    void SetupSwapChain();
    void CreateCommandBuffers();
    void CreateFences();
    void SetupDepthStencil();
    void SetupRenderPass();
    void CreatePipelineCache();
    void SetupFrameBuffer();

    // Cleanup steps
    void CleanupSDL();
    void DestroyCommandBuffers();
    void SetupMultisampleTarget();

    bool CheckValidationLayersSupport();

    void PollEvents(float deltaTime);

    bool SetupDependencies();
    void UpdateSystems(float deltaTime);

    void PrepareImgui();

    void WindowResize();

    // TODO: move to some renderer interface
    VkShaderModule LoadSPIRVShader(const std::string& filename) const;

    bool isRunning = false;

    std::vector<VkShaderModule> shaderModules;
};
}
