#include "vulkan\vulkan_core.h"

struct SDL_Window;

namespace CG
{
    struct engine_config;

    class engine
    {
    public:
        engine(const CG::engine_config& engine_config);

        void run();

    private:
        bool init();
        void main_loop();
        void cleanup();

        bool init_SDL();
        bool init_window();
        bool init_graphics_api();
        bool init_surface();

        void SDL_cleanup();

        const CG::engine_config& engine_config;

        SDL_Window* window;

        VkInstance vk_instance;
    };
}