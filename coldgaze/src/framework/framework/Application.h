#pragma once

#include "Forwards.hpp"
#include "VScopedPtr.hpp"

class DevicePicker;
class QueueSelector;

// TODO: think about good error delivery, get rid of try catch
class Application
{
public:
	Application();
	~Application();

	int run();
	static std::vector<VkExtensionProperties> get_available_extensions();
	static bool check_validation_layer_support();
	static std::vector<const char*> get_required_extension();

private:
	int _init_window();

	int _init_vulkan();
	int _create_instance();
	int _create_surface();
	// TODO: move in class
	int _create_logical_device();
	int _try_setup_debug_callback();

	int _main_loop();

	VScopedPtr<VkInstance> _instance;
	VScopedPtr<VkDevice> _logical_device;
	VScopedPtr<VkDebugReportCallbackEXT> _callback;
	VScopedPtr<VkSurfaceKHR> _surface;
	GLFWwindow* _window = nullptr;
	VkQueue _graphics_queue;
	VkPhysicalDeviceFeatures _device_features;

	std::unique_ptr<DevicePicker> _picker;
	std::unique_ptr<QueueSelector> _queue_selector;

	int _width = 0;
	int _height = 0;
};

