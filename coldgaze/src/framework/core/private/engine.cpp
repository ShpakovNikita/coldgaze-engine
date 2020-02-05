#include "Core/Engine.hpp"
#include "Core/EngineConfig.hpp"
#include "SDL2/SDL_video.h"
#include "SDL2/SDL_vulkan.h"
#include <vector>
#include "vulkan/vulkan_core.h"
#include "SDL2/SDL.h"
#include <iostream>
#include "Utils/Vulkan.hpp"


CG::Engine::Engine(const CG::EngineConfig& a_engine_config)
    : engineConfig(a_engine_config)
{

}

void CG::Engine::run()
{
    isRunning = init();

    while (isRunning)
    {
        main_loop();
    }
    
    cleanup();
}

bool CG::Engine::init()
{
    return init_SDL() && init_window() && init_graphics_api() && init_surface();
}

void CG::Engine::main_loop()
{
    SDL_poll_events();
}

void CG::Engine::cleanup()
{

}

bool CG::Engine::init_SDL()
{
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) >= 0;
}

bool CG::Engine::init_window()
{
    window = SDL_CreateWindow(
        engineConfig.engine_name.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        engineConfig.width, engineConfig.height,
        SDL_WINDOW_VULKAN
    );

    return window != nullptr;
}

bool CG::Engine::init_graphics_api()
{
    uint32_t extensionCount = 0;
    bool extensions_found = SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = engineConfig.engine_name.c_str();
    appInfo.pEngineName = engineConfig.engine_name.c_str();
    appInfo.apiVersion = engineConfig.vk_api_version;

    std::vector<const char*> layerNames {};

#if ENABLE_VULKAN_VALIDATION
    extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledLayerCount = layerNames.size();
    info.ppEnabledLayerNames = layerNames.data();
    info.enabledExtensionCount = extensionNames.size();
    info.ppEnabledExtensionNames = extensionNames.data();

    return vkCreateInstance(&info, nullptr, &vkInstance) == VK_SUCCESS;
}

bool CG::Engine::setup_debugging()
{
#if ENABLE_VULKAN_VALIDATION
    VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    return CG::VkUtils::Debug::SetupDebugging(vkInstance, debugReportFlags, VK_NULL_HANDLE);
#endif

    return true;
}

bool CG::Engine::init_surface()
{
    return SDL_Vulkan_CreateSurface(window, vkInstance, &surface);
}

void CG::Engine::SDL_cleanup()
{
    SDL_DestroyWindow(window);

    SDL_Quit();
}

void CG::Engine::SDL_poll_events()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_WINDOWEVENT:
        {
            // pass
        }
        break;

        case SDL_KEYDOWN:
        {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);

            if (event.key.keysym.sym == SDLK_ESCAPE) {
                isRunning = false;
            }
        }
        break;

        case SDL_MOUSEBUTTONDOWN: 
        {
            // pass
        }
        break;

        case SDL_QUIT: 
        {
            isRunning = false;
        }
        break;
        }
    }
}

