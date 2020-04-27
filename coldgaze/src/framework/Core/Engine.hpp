#pragma once
#include "vulkan\vulkan_core.h"
#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <chrono>
#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

struct SDL_Window;
class ICGSystem;
union SDL_Event;

namespace CG
{
    namespace Vk 
    {
        class SwapChain;
        class Device;
    }
    struct EngineConfig;
	class InputHandler;
	class Window;

    class Engine
    {
    public:
        Engine(CG::EngineConfig& engineConfig);
		virtual ~Engine();

        void Run();

		entt::registry& GetRegistry();
		const Vk::Device* GetDevice() const;

		const InputHandler* GetInputHandler() const;
		const Window* GetCurrentWindow() const;

		// TODO: move to some renderer interface
		VkPipelineShaderStageCreateInfo LoadShader(const std::string& filename, VkShaderStageFlagBits stage);
		const std::string GetAssetPath() const;

	protected:
        virtual VkPhysicalDeviceFeatures GetEnabledDeviceFeatures();
        virtual void RenderFrame(float deltaTime);
        virtual void Prepare();
		virtual void Cleanup();

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
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties = {};

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

		std::unique_ptr<InputHandler> inputHandler;
		std::unique_ptr<Window> currentWindow;

    private:
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

		bool CheckValidationLayersSupport();

        void PollEvents(float deltaTime);

		bool SetupDependencies();
		void UpdateSystems(float deltaTime);

		// TODO: move to some renderer interface
		VkShaderModule LoadSPIRVShader(const std::string& filename) const;

        bool isRunning = false;
		std::chrono::time_point<std::chrono::steady_clock> previousTime;

		std::vector<VkShaderModule> shaderModules;
    };
}