#include "Application.h"
#include "VScopedPtr.hpp"

#include <functional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "VulkanDebugCallbacks.hpp"
#include "DevicePicker.h"
#include "QueueSelector.h"
#include "Renderer.h"

namespace SApplication
{
#ifdef NDEBUG
	const bool enable_validation_layers = false;
#else
	const bool enable_validation_layers = true;
#endif

	static const std::vector<const char*> validation_layers = {
	"VK_LAYER_LUNARG_standard_validation"
	};
}

Application::Application()
	: _instance(vkDestroyInstance)
	, _logical_device(vkDestroyDevice)
 	, _callback(_instance, DestroyDebugReportCallbackEXT)
{
}

Application::~Application()
{
}

int Application::run()
{
	// TODO: proper result check, logging and output 
	int result = _init_vulkan();
	result |= _main_loop();

	return result;
}

std::vector<VkExtensionProperties> Application::get_available_extensions()
{
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

	return extensions;
}

bool Application::check_validation_layer_support()
{
	using namespace SApplication;

	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

	std::vector<VkLayerProperties> available_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

	for (const char* layer : validation_layers)
	{
		bool layer_found = false;

		for (const auto& layer_properties : available_layers) {
			if (strcmp(layer, layer_properties.layerName) == 0) {
				layer_found = true;
				break;
			}
		}

		if (!layer_found) {
			return false;
		}
	}

	return true;
}

std::vector<const char*> Application::get_required_extension()
{
	using namespace SApplication;

	std::vector<const char*> extensions;

	unsigned int glfw_extension_count = 0;
	const char** glfw_extensions;

	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	
	for (unsigned int i = 0; i < glfw_extension_count; i++) {
		extensions.push_back(glfw_extensions[i]);
	}

	if (enable_validation_layers) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return extensions;
}

int Application::_init_vulkan()
{
	using namespace SApplication;

	if (enable_validation_layers && !check_validation_layer_support()) {
		throw std::runtime_error("validation layers requested, but not available!");
		return VK_ERROR_LAYER_NOT_PRESENT;
	}

	_create_instance();
	_try_setup_debug_callback();

	_window = std::make_unique<CG::Window>(_instance);

	VkSurfaceKHR surface = _window->create_surface(eRenderApi::vulkan);
	_renderer = std::make_unique<CG::Renderer>(surface);

	_picker = std::make_unique<DevicePicker>(_instance);
	_picker->pick_best_device();

	_queue_selector = std::make_unique<QueueSelector>(_picker->get_device());

	_create_logical_device();

	return VK_SUCCESS;
}

int Application::_create_instance()
{
	using namespace SApplication;

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Hello Triangle";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	std::vector<const char*> glfwExtensions = get_required_extension();

	create_info.enabledExtensionCount = glfwExtensions.size();
	create_info.ppEnabledExtensionNames = glfwExtensions.data();

	if (enable_validation_layers) {
		create_info.enabledLayerCount = validation_layers.size();
		create_info.ppEnabledLayerNames = validation_layers.data();
	}
	else {
		create_info.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&create_info, nullptr, _instance.replace()) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	return VK_SUCCESS;
}

int Application::_create_logical_device()
{
	using namespace SApplication;

	QueueFamilyIndices indices = _queue_selector->get_queue_family_indices();

	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = indices.graphics_family;
	queue_create_info.queueCount = 1;

	float queue_priority = 1.0f;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.queueCreateInfoCount = 1;

	create_info.pEnabledFeatures = &_device_features;

	create_info.enabledExtensionCount = 0;

	if (enable_validation_layers) {
		create_info.enabledLayerCount = validation_layers.size();
		create_info.ppEnabledLayerNames = validation_layers.data();
	}
	else {
		create_info.enabledLayerCount = 0;
	}

	if (vkCreateDevice(_picker->get_device(), &create_info, nullptr, _logical_device.replace()) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
		vkGetDeviceQueue(_logical_device, indices.graphics_family, 0, &_graphics_queue);
		return VK_ERROR_DEVICE_LOST;
	}

	return CG_INIT_SUCCESS;
}

int Application::_try_setup_debug_callback()
{
	using namespace SApplication;

	if (!enable_validation_layers)
	{
		return VK_NOT_READY;
	}

	VkDebugReportCallbackCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	create_info.pfnCallback = debug_callback;

	if (CreateDebugReportCallbackEXT(_instance, &create_info, nullptr, _callback.replace()) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug callback!");
		return VK_ERROR_INITIALIZATION_FAILED;
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

	while (_window->is_window_alive()) {
		_window->poll_events();
	}
	_window->terminate();

	return CG_INIT_SUCCESS;
}
