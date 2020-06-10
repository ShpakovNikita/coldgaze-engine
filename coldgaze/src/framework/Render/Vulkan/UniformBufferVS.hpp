#pragma once
#include "Buffer.hpp"
#include "vulkan/vulkan_core.h"

namespace CG {
namespace Vk {
    class Device;

    struct UniformBufferVS {
        Buffer buffer = {};

        VkDescriptorBufferInfo descriptor;

        void PrepareUniformBuffers(Device* vkDevice, VkDeviceSize size);
    };
}
}
