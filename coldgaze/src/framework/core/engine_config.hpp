#include "stdint.h"

#define ENABLE_VULKAN_VALIDATION 1

namespace CG
{
    struct engine_config
    {
        uint32_t width = 1280;
        uint32_t height = 720;
    };
}