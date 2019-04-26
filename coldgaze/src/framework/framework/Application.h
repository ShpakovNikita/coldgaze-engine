#pragma once

#include "Forwards.hpp"
#include "VScopedPtr.hpp"
#include "Window.h"

class DevicePicker;
class QueueSelector;

// TODO: think about good error delivery, get rid of try catch, make global engine context
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
	int _init_vulkan();
	int _create_instance();

	// TODO: move in class
	int _create_logical_device();
	int _try_setup_debug_callback();

	int _main_loop();

	VScopedPtr<VkInstance> _instance;
	VScopedPtr<VkDevice> _logical_device;
	VScopedPtr<VkDebugReportCallbackEXT> _callback;
	VkQueue _graphics_queue;
	VkPhysicalDeviceFeatures _device_features;

	std::unique_ptr<CG::Renderer> _renderer;
	std::unique_ptr<CG::Window> _window;
	std::unique_ptr<DevicePicker> _picker;
	std::unique_ptr<QueueSelector> _queue_selector;
};

