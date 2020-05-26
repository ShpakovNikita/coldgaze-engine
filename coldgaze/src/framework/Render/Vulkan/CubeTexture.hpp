#pragma once

#include <string>
#include "Texture.hpp"

namespace CG
{
    namespace Vk
    {
        class Device;

        class CubeTexture
            : public Texture
        {
        public:
            void LoadFromFile(const std::string& fileName, Device* device, VkQueue copyQueue);

        private:
            void FromBuffer(
                const void* buffer,
                VkDeviceSize bufferSize,
                VkFormat format,
                uint32_t texWidth,
                uint32_t texHeight,
                Device* device,
                VkQueue copyQueue,
                VkFilter filter = VK_FILTER_LINEAR,
                VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
                VkImageLayout aImageLayout= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        };
    }
}
