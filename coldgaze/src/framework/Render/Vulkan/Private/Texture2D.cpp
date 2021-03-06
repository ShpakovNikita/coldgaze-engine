#include "Render/Vulkan/Texture2D.hpp"
#include "Render/Vulkan/Debug.hpp"
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Exceptions.hpp"
#include "Render/Vulkan/Initializers.hpp"
#include "Render/Vulkan/Utils.hpp"
#include "stb_image.h"

void CG::Vk::Texture2D::LoadFromFile(const std::string& fileName, Device* device, VkQueue copyQueue, bool loadHDR /*= false*/)
{
    vkDevice = device;

    int imageWidth, imageHeight, nrComponents;
    void* data;
    if (loadHDR) {
        stbi_set_flip_vertically_on_load(true);
        data = stbi_loadf(fileName.c_str(), &imageWidth, &imageHeight, &nrComponents, 0);
        stbi_set_flip_vertically_on_load(false);
    } else {
        data = stbi_load(fileName.c_str(), &imageWidth, &imageHeight, &nrComponents, 0);
    }

    if (data) {
        width = static_cast<uint32_t>(imageWidth);
        height = static_cast<uint32_t>(imageHeight);
        uint32_t imageSize = 0;

        if (loadHDR) {
            imageSize = width * height * 3 * sizeof(float);

            TextureSampler loadingSampler;
            loadingSampler.magFilter = VK_FILTER_LINEAR;
            loadingSampler.minFilter = VK_FILTER_LINEAR;
            loadingSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            loadingSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            loadingSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

            FromBuffer(data, imageSize, VK_FORMAT_R32G32B32_SFLOAT, width, height, device,
                copyQueue, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_TILING_LINEAR, loadingSampler);
        } else {
            imageSize = width * height * 4 * sizeof(unsigned char);
            FromBuffer(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, width, height, device, copyQueue);
        }

        stbi_image_free(data);
    } else {
        throw AssetLoadingException("Failed to load image! Make sure that it has RGBE or RGB format!");
    }
}

void CG::Vk::Texture2D::FromBuffer(
    const void* buffer,
    VkDeviceSize bufferSize,
    VkFormat format,
    uint32_t texWidth,
    uint32_t texHeight,
    Device* device,
    VkQueue copyQueue,
    VkImageUsageFlags imageUsageFlags /*= VK_IMAGE_USAGE_SAMPLED_BIT*/,
    VkImageLayout aImageLayout /*= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL*/,
    VkImageTiling imageTiling /*= VK_IMAGE_TILING_OPTIMAL*/,
    TextureSampler textureSampler /*= {}*/)
{
    vkDevice = device;
    width = texWidth;
    height = texHeight;
    mipLevels = 1;
    imageLayout = aImageLayout;

    VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = Initializers::BufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSize);
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RESULT(vkCreateBuffer(vkDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

    VkMemoryAllocateInfo memAllocInfo = Initializers::MemoryAllocateInfo();
    VkMemoryRequirements memReqs;

    vkGetBufferMemoryRequirements(vkDevice->logicalDevice, stagingBuffer, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->GetMemoryTypeIndex(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(vkBindBufferMemory(vkDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

    void* data;
    VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
    memcpy(data, buffer, bufferSize);
    vkUnmapMemory(device->logicalDevice, stagingMemory);

    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    VkImageCreateInfo imageCreateInfo = Initializers::ImageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = imageTiling;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { width, height, 1 };
    imageCreateInfo.usage = imageUsageFlags;
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    VK_CHECK_RESULT(vkCreateImage(vkDevice->logicalDevice, &imageCreateInfo, nullptr, &image));

    vkGetImageMemoryRequirements(vkDevice->logicalDevice, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;

    memAllocInfo.memoryTypeIndex = vkDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(vkDevice->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
    VK_CHECK_RESULT(vkBindImageMemory(vkDevice->logicalDevice, image, deviceMemory, 0));

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.layerCount = 1;

    Utils::SetImageLayout(
        copyCmd,
        image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    vkCmdCopyBufferToImage(
        copyCmd,
        stagingBuffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion);

    Utils::SetImageLayout(
        copyCmd,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        imageLayout,
        subresourceRange);

    vkDevice->FlushCommandBuffer(copyCmd, copyQueue);
    vkFreeMemory(vkDevice->logicalDevice, stagingMemory, nullptr);
    vkDestroyBuffer(vkDevice->logicalDevice, stagingBuffer, nullptr);

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = textureSampler.magFilter;
    samplerCreateInfo.minFilter = textureSampler.minFilter;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = textureSampler.addressModeU;
    samplerCreateInfo.addressModeV = textureSampler.addressModeV;
    samplerCreateInfo.addressModeW = textureSampler.addressModeW;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    VK_CHECK_RESULT(vkCreateSampler(vkDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext = NULL;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.image = image;
    VK_CHECK_RESULT(vkCreateImageView(vkDevice->logicalDevice, &viewCreateInfo, nullptr, &view));

    UpdateDescriptor();
}
