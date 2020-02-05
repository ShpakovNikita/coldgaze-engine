#include "vulkan\vulkan_core.h"

struct SDL_Window;

namespace CG
{
    struct EngineConfig;

    class Engine
    {
    public:
        Engine(const CG::EngineConfig& engineConfig);

        void run();

    private:
        bool init();
        void main_loop();
        void cleanup();

        bool init_SDL();
        bool init_window();
        bool init_graphics_api();
        bool setup_debugging();
        bool init_surface();

        void SDL_cleanup();
        void SDL_poll_events();

        const CG::EngineConfig& engineConfig;

        SDL_Window* window;

        VkInstance vkInstance;
        VkSurfaceKHR surface;

        bool isRunning = false;
    };
}