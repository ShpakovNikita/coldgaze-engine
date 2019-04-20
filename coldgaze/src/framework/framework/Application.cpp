#include "Application.h"
#include "VScopedPtr.hpp"

#include <vulkan/vulkan.h>
#include <functional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>


#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

namespace SApplication
{
	static const int width = 800;
	static const int height = 600;

	static const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
	};
}

Application::Application()
	: _instance(vkDestroyInstance)
{
}

Application::~Application()
{
}

int Application::run()
{
	// TODO: proper result check, logging and output 
	_init_window();
	_init_vulkan();
	_main_loop();

#ifdef _WIN32
	return EXIT_SUCCESS;
#elif __linux__ 
	return 0;
#endif
}

std::vector<VkExtensionProperties> Application::get_available_extensions()
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	return extensions;
}

bool Application::check_validation_layer_support()
{
	using namespace SApplication;

	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layer : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layer, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

int Application::_init_window()
{
	using namespace SApplication;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	_width = width;
	_height = height;

	_window = glfwCreateWindow(_width, _height, "Vulkan window", nullptr, nullptr);

	// working glm test
	glm::mat4 matrix;
	glm::vec4 vec;
	auto test = matrix * vec;

	return CG_INIT_SUCCESS;
}

int Application::_init_vulkan()
{
	using namespace SApplication;

	if (enableValidationLayers && !check_validation_layer_support()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&createInfo, nullptr, _instance.replace()) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}

	return VK_SUCCESS;
}

int Application::_main_loop()
{
	std::cout << "available extensions:" << std::endl;
	std::vector<VkExtensionProperties> extensions = get_available_extensions();

	for (const auto& extension : extensions) {
		std::cout << "\t" << extension.extensionName << std::endl;
	}

	while (!glfwWindowShouldClose(_window)) {
		glfwPollEvents();
	}

	glfwDestroyWindow(_window);

	glfwTerminate();

	return CG_INIT_SUCCESS;
}
