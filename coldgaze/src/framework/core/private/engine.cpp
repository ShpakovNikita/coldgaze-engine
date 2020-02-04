#include "core/engine.hpp"
#include "core/engine_config.hpp"
#include "SDL2/SDL_video.h"
#include "SDL2/SDL_vulkan.h"
#include <vector>
#include "vulkan/vulkan_core.h"
#include "SDL2/SDL.h"


CG::engine::engine(const CG::engine_config& a_engine_config)
    : engine_config(a_engine_config)
{

}

void CG::engine::run()
{
    bool is_running = init();

    while (is_running)
    {
        main_loop();
    }
    
    cleanup();
}

bool CG::engine::init()
{
    return init_SDL() && init_window() && init_graphics_api() && init_surface();
}

void CG::engine::main_loop()
{

}

void CG::engine::cleanup()
{

}

bool CG::engine::init_SDL()
{
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) >= 0;
}

bool CG::engine::init_window()
{
    SDL_Window* window = SDL_CreateWindow(
        "Vulkan_Sample",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        engine_config.width, engine_config.height,
        SDL_WINDOW_VULKAN
    );

    return window != nullptr;
}

bool CG::engine::init_graphics_api()
{
    uint32_t extensionCount = 0;
    bool extensions_found = SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

    VkApplicationInfo appInfo{};
    // TODO: fill this out

    std::vector<const char*> layerNames{};
    // uncomment below if you want to use validation layers
    // layerNames.push_back("VK_LAYER_LUNARG_standard_validation");

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledLayerCount = layerNames.size();
    info.ppEnabledLayerNames = layerNames.data();
    info.enabledExtensionCount = extensionNames.size();
    info.ppEnabledExtensionNames = extensionNames.data();

    VkResult res;
    VkInstance instance;
    res = vkCreateInstance(&info, nullptr, &instance);
    return res == VK_SUCCESS;
}

bool CG::engine::init_surface()
{
    VkSurfaceKHR surface;
    return SDL_Vulkan_CreateSurface(window, vk_instance, &surface);
}

void CG::engine::SDL_cleanup()
{
    SDL_DestroyWindow(window);

    SDL_Quit();
}

