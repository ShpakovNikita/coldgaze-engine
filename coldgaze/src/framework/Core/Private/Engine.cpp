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
	CleanupSDL();
	delete vkDevice;
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
		CreateDevices();
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

#if !defined(VK_USE_PLATFORM_ANDROID_KHR)	
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
#endif

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
	VK_CHECK_RESULT(vkDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain));

    VkDevice device = vkDevice->logicalDevice;

    vkGetDeviceQueue(device, vkDevice->queueFamilyIndices.graphics, 0, &queue);

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

