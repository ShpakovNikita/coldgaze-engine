#include "Render/Vulkan/Texture.hpp"
#include "Render/Vulkan/Device.hpp"

CG::Vk::Texture::~Texture()
{
    Destroy();
}

void CG::Vk::Texture::UpdateDescriptor()
{
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = imageLayout;
}

void CG::Vk::Texture::Destroy()
{
    vkDestroyImageView(vkDevice->logicalDevice, view, nullptr);
    vkDestroyImage(vkDevice->logicalDevice, image, nullptr);
    if (sampler)
    {
        vkDestroySampler(vkDevice->logicalDevice, sampler, nullptr);
    }
    vkFreeMemory(vkDevice->logicalDevice, deviceMemory, nullptr);
}

