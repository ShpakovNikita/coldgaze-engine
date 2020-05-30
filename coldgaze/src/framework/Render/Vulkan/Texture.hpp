#pragma once

#include "vulkan\vulkan_core.h"

namespace CG
{
    namespace Vk
    {
        class Device;

        class Texture
        {
        public:
            VkDescriptorImageInfo descriptor = {};

            virtual ~Texture();

            const Device* vkDevice = nullptr;
            uint32_t height = 0, width = 0;
            uint16_t mipLevels = 0;
            VkImage image = {};
            VkDeviceMemory deviceMemory = {};
            VkImageLayout imageLayout = {};
            VkSampler sampler = {};
            VkImageView view = {};

            void Destroy();

        protected:
            void UpdateDescriptor();
        };
    }
}