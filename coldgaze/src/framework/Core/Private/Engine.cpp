#include "Core/Engine.hpp"
#include "Core/EngineConfig.hpp"
#include "SDL2/SDL_video.h"
#include "SDL2/SDL_vulkan.h"
#include <vector>
#include "vulkan/vulkan_core.h"
#include "SDL2/SDL.h"
#include <iostream>
#include "Render/Vulkan/Debug.hpp"
#include <assert.h>
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/SwapChain.hpp"
#include <array>


CG::Engine::Engine(CG::EngineConfig& a_engine_config)
    : engineConfig(a_engine_config)
{
	// pass
}

VkPhysicalDeviceFeatures CG::Engine::GetEnabledDeviceFeatures()
{
	return {};
}

void CG::Engine::Run()
{
    isRunning = Init();

	if (isRunning)
	{
		Prepare();
	}

    while (isRunning)
    {
        MainLoop();
    }
    
    Cleanup();
}

bool CG::Engine::Init()
{
    return InitSDL() && InitWindow() && InitGraphicsAPI();
}

void CG::Engine::Prepare()
{
	// TODO: read about debug markers
	InitSwapChain();
	CreateCommandPool();
	SetupSwapChain();
	CreateCommandBuffers();
	CreateFences();
	SetupDepthStencil();
	SetupRenderPass();
	CreatePipelineCache();
	SetupFrameBuffer();
}

void CG::Engine::MainLoop()
{
    PollEvents();
}

void CG::Engine::Cleanup()
{
	DestroyCommandBuffers();
	delete vkSwapChain;
	delete vkDevice;
	vkDestroyInstance(vkInstance, nullptr);
	CleanupSDL();
}

bool CG::Engine::InitSDL()
{
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) >= 0;
}

bool CG::Engine::InitWindow()
{
    window = SDL_CreateWindow(
        engineConfig.engine_name.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        engineConfig.width, engineConfig.height,
        SDL_WINDOW_VULKAN
    );

    return window != nullptr;
}

bool CG::Engine::InitGraphicsAPI()
{
	return CreateVkInstance() &&
		SetupDebugging() &&
		CreateDevices() && 
		CreateSwapChain() &&
		SetupSemaphores();
}

bool CG::Engine::SetupDebugging()
{
#if ENABLE_VULKAN_VALIDATION
    VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    return CG::Vk::Debug::SetupDebugging(vkInstance, debugReportFlags, VK_NULL_HANDLE);
#else
    return true;
#endif
}

bool CG::Engine::CreateDevices()
{
	uint32_t gpuCount = 0;

	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vkInstance, &gpuCount, nullptr));
	assert(gpuCount > 0);

	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	VkResult err = vkEnumeratePhysicalDevices(vkInstance, &gpuCount, physicalDevices.data());
	VK_CHECK_RESULT(err);

	// List available GPUs
	uint32_t selectedDevice = 0;
	
	// GPU selection via command line argument
	for (size_t i = 0; i < engineConfig.args.size(); ++i)
	{
		// Select GPU
		if ((engineConfig.args[i] == std::string("-g")) || (engineConfig.args[i] == std::string("-gpu")))
		{
			char* endptr;
			uint32_t index = strtol(engineConfig.args[i + 1], &endptr, 10);
			if (endptr != engineConfig.args[i + 1])
			{
				if (index > gpuCount - 1)
				{
					std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << std::endl;
				}
				else
				{
					std::cout << "Selected Vulkan device " << index << std::endl;
					selectedDevice = index;
				}
			};
			break;
		}

		if (engineConfig.args[i] == std::string("-listgpus"))
		{
			if (gpuCount == 0)
			{
				std::cerr << "No Vulkan devices found!" << std::endl;
			}
			else
			{
				for (uint32_t j = 0; j < gpuCount; ++j) {
					VkPhysicalDeviceProperties deviceProperties;
					vkGetPhysicalDeviceProperties(physicalDevices[j], &deviceProperties);
					std::cout << "Device [" << j << "] : " << deviceProperties.deviceName << std::endl;
					std::cout << " Type: " << CG::Vk::Debug::PhysicalDeviceTypeString(deviceProperties.deviceType) << std::endl;
					std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << std::endl;
				}
			}
		}
	}

	// TODO: use later
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;

    std::vector<const char*> enabledDeviceExtensions;
	void* deviceCreatepNextChain = nullptr;

	vkPhysicalDevice = physicalDevices[selectedDevice];

	vkGetPhysicalDeviceProperties(vkPhysicalDevice, &deviceProperties);
	vkGetPhysicalDeviceFeatures(vkPhysicalDevice, &deviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &deviceMemoryProperties);

	VkPhysicalDeviceFeatures enabledFeatures = GetEnabledDeviceFeatures();

    vkDevice = new CG::Vk::Device(vkPhysicalDevice);
	VK_CHECK_RESULT(vkDevice->CreateLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain));

    VkDevice device = vkDevice->logicalDevice;

    vkGetDeviceQueue(device, vkDevice->queueFamilyIndices.graphics, 0, &queue);

	VkFormat depthFormat;
    VkBool32 validDepthFormat = vkDevice->GetSupportedDepthFormat(vkPhysicalDevice, &depthFormat);
    assert(validDepthFormat);

	return true;
}

bool CG::Engine::CreateSwapChain()
{
	VkDevice device = vkDevice->logicalDevice;

    vkSwapChain = new CG::Vk::SwapChain(vkInstance, vkPhysicalDevice, device);

	return true;
}

bool CG::Engine::SetupSemaphores()
{
	VkDevice device = vkDevice->logicalDevice;

    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands have been sumbitted and executed
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));

	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &submitPipelineStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphores.presentComplete;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphores.renderComplete;

	return true;
}

void CG::Engine::InitSwapChain()
{
	vkSwapChain->InitSurface(window);
}

void CG::Engine::CreateCommandPool()
{
	VkDevice device = vkDevice->logicalDevice;
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = vkSwapChain->queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &vkCmdPool));
}

void CG::Engine::SetupSwapChain()
{
	vkSwapChain->Create(&engineConfig.width, &engineConfig.height, engineConfig.vsync);
}

void CG::Engine::CreateCommandBuffers()
{
	VkDevice device = vkDevice->logicalDevice;

	drawCmdBuffers.resize(vkSwapChain->imageCount);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = vkCmdPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(drawCmdBuffers.size());

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, drawCmdBuffers.data()));
}

void CG::Engine::CreateFences()
{
	VkDevice device = vkDevice->logicalDevice;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	waitFences.resize(drawCmdBuffers.size());
	for (auto& fence : waitFences) {
		VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
	}
}

void CG::Engine::SetupDepthStencil()
{
	VkDevice device = vkDevice->logicalDevice;

	VkImageCreateInfo imageCI = {};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = vkDevice->depthFormat;
	imageCI.extent = { engineConfig.width, engineConfig.height, 1 };
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

	VkMemoryAllocateInfo memAllloc{};
	memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllloc.allocationSize = memReqs.size;
	memAllloc.memoryTypeIndex = vkDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.image = depthStencil.image;
	imageViewCI.format = vkDevice->depthFormat;
	imageViewCI.subresourceRange.baseMipLevel = 0;
	imageViewCI.subresourceRange.levelCount = 1;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount = 1;
	imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	// Stencil aspect should only be set on depth + stencil formats VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	if (vkDevice->depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
		imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));
}

void CG::Engine::SetupRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};

	attachments[0].format = vkSwapChain->colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attachments[1].format = vkDevice->depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(vkDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass));
}

void CG::Engine::CreatePipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(vkDevice->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void CG::Engine::SetupFrameBuffer()
{
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = engineConfig.width;
	frameBufferCreateInfo.height = engineConfig.height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	frameBuffers.resize(vkSwapChain->imageCount);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		attachments[0] = vkSwapChain->buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(vkDevice->logicalDevice, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
	}
}

bool CG::Engine::CreateVkInstance()
{
	uint32_t extensionCount = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr))
	{
		return false;
	}

	std::vector<const char*> extensionNames(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = engineConfig.engine_name.c_str();
	appInfo.pEngineName = engineConfig.engine_name.c_str();
	appInfo.apiVersion = engineConfig.vk_api_version;

	std::vector<const char*> layerNames{};

#if ENABLE_VULKAN_VALIDATION
	extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

	VkInstanceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &appInfo;
	info.enabledLayerCount = static_cast<uint32_t>(layerNames.size());
	info.ppEnabledLayerNames = layerNames.data();
	info.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
	info.ppEnabledExtensionNames = extensionNames.data();

	return vkCreateInstance(&info, nullptr, &vkInstance) == VK_SUCCESS;
}

void CG::Engine::CleanupSDL()
{
    SDL_DestroyWindow(window);

    SDL_Quit();
}

void CG::Engine::DestroyCommandBuffers()
{
	VkDevice device = vkDevice->logicalDevice;
	vkFreeCommandBuffers(device, vkCmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void CG::Engine::PollEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_WINDOWEVENT:
        {
            // pass
        }
        break;

        case SDL_KEYDOWN:
        {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                isRunning = false;
            }
        }
        break;

        case SDL_MOUSEBUTTONDOWN: 
        {
            // pass
        }
        break;

        case SDL_QUIT: 
        {
            isRunning = false;
        }
        break;
        }
    }
}

