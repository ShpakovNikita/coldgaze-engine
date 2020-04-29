#include "Render/Vulkan/Texture2D.hpp"

void CG::Vk::Texture2D::FromBuffer(
	const void* buffer, 
	VkDeviceSize bufferSize, 
	VkFormat format, 
	uint32_t texWidth,
	uint32_t texHeight,
	Device* device,
	VkQueue copyQueue, 
	VkFilter filter /*= VK_FILTER_LINEAR*/, 
	VkImageUsageFlags imageUsageFlags /*= VK_IMAGE_USAGE_SAMPLED_BIT*/,
	VkImageLayout imageLayout /*= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL*/)
{

}

