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


CG::Engine::Engine(const CG::EngineConfig& a_engine_config)
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

    while (isRunning)
    {
        MainLoop();
    }
    
    Cleanup();
}

bool CG::Engine::Init()
{
    return InitSDL() && InitWindow() && InitGraphicsAPI() && InitSurface();
}

void CG::Engine::MainLoop()
{
    PollEvents();
}

void CG::Engine::Cleanup()
{
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

bool CG::Engine::InitSurface()
{
    return SDL_Vulkan_CreateSurface(window, vkInstance, &surface);
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

