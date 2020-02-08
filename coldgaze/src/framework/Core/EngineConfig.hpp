#include "stdint.h"
#include <string>
#include <vector>

#define ENABLE_VULKAN_VALIDATION 1

namespace CG
{
    struct EngineConfig
    {
        uint32_t width = 1280;
        uint32_t height = 720;

		bool vsync = false;

        uint32_t vk_api_version = VK_API_VERSION_1_0;

        std::string engine_name = "Coldgaze";

		std::vector<const char*> args;
    };
}