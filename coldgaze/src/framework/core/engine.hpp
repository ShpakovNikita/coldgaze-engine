#include "vulkan\vulkan_core.h"

struct SDL_Window;

namespace CG
{
    struct EngineConfig;

    class Engine
    {
    public:
        Engine(const CG::EngineConfig& engineConfig);

		virtual void EnableDeviceFeatures();

        void Run();

    private:
        bool Init();
        void MainLoop();
        void Cleanup();

        bool InitSDL();
        bool InitWindow();
        bool InitGraphicsAPI();
        bool InitSurface();

		bool CreateVkInstance();
		bool SetupDebugging();
		bool CreateDevices();

        void CleanupSDL();
        void PollEvents();

        const CG::EngineConfig& engineConfig;

        SDL_Window* window = nullptr;

		VkPhysicalDevice vkPhysicalDevice = {};
		VkInstance vkInstance = {};
		VkSurfaceKHR surface = {};

        bool isRunning = false;
    };
}