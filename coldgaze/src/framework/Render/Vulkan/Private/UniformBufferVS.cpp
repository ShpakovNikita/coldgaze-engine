#pragma once
#include "Render/Vulkan/UniformBufferVS.hpp"
#include "Render/Vulkan/Debug.hpp"
#include "Render/Vulkan/Device.hpp"

using namespace CG::Vk;

void CG::Vk::UniformBufferVS::PrepareUniformBuffers(Device* vkDevice, VkDeviceSize size)
{
    VK_CHECK_RESULT(vkDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &buffer,
        size));

    VK_CHECK_RESULT(buffer.Map());
}
