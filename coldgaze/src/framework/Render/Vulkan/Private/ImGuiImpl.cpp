#include "Render/Vulkan/ImGuiImpl.hpp"
#include "core/engine.hpp"
#include "imgui/imgui.h"
#include "Render/Vulkan/Initializers.hpp"
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Debug.hpp"

using namespace CG;

CG::Vk::ImGuiImpl::ImGuiImpl(const Engine& aEngine)
	: engine(aEngine)
	, device(aEngine.GetDevice())
{
	ImGui::CreateContext();
}

CG::Vk::ImGuiImpl::~ImGuiImpl()
{
	ImGui::DestroyContext();

	vkDestroyImage(device->logicalDevice, fontImage, nullptr);
	vkFreeMemory(device->logicalDevice, fontMemory, nullptr);
}

void CG::Vk::ImGuiImpl::Init(float width, float height)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(width, height);
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void CG::Vk::ImGuiImpl::InitResources([[ maybe_unused ]] VkRenderPass renderPass, [[ maybe_unused ]] VkQueue queue)
{
	ImGuiIO& io = ImGui::GetIO();

	// Create font texture
	unsigned char* fontData;
	int texWidth, texHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
	[[ maybe_unused ]] VkDeviceSize uploadSize = static_cast<uint64_t>(texWidth) * static_cast<uint64_t>(texHeight) * 4 * sizeof(char);

	VkImageCreateInfo imageInfo = CG::Vk::Initializers::ImageCreateInfo();
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent.width = texWidth;
	imageInfo.extent.height = texHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageInfo, nullptr, &fontImage));
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device->logicalDevice, fontImage, &memReqs);
	VkMemoryAllocateInfo memAllocInfo = CG::Vk::Initializers::MemoryAllocateInfo();
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &fontMemory));
	VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, fontImage, fontMemory, 0));
}

