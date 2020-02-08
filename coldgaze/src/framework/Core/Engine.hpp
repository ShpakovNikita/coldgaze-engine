#include "vulkan\vulkan_core.h"

struct SDL_Window;

namespace CG
{
    namespace Vk 
    {
        class SwapChain;
        class Device;
    }
    struct EngineConfig;

    class Engine
    {
    public:
        Engine(const CG::EngineConfig& engineConfig);

		virtual VkPhysicalDeviceFeatures GetEnabledDeviceFeatures();

        void Run();

    private:
        bool Init();
		virtual void Prepare();
        void MainLoop();
        void Cleanup();

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

		// Cleanup steps
        void CleanupSDL();

        void PollEvents();

        const CG::EngineConfig& engineConfig;

        SDL_Window* window = nullptr;
        Vk::Device* vkDevice = nullptr;
        Vk::SwapChain* vkSwapChain = nullptr;
        VkQueue queue = {};

		VkPhysicalDevice vkPhysicalDevice = {};
		VkInstance vkInstance = {};
		VkSurfaceKHR surface = {};
		VkCommandPool vkCmdPool = {};

        struct {
            // Swap chain image presentation
            VkSemaphore presentComplete;
            // Command buffer submission and execution
            VkSemaphore renderComplete;
		} semaphores = {};

        bool isRunning = false;
    };
}