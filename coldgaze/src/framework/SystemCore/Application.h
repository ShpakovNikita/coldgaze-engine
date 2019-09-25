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

	void run();
	static std::vector<VkExtensionProperties> get_available_extensions();
	static bool check_validation_layer_support();
	static std::vector<const char*> get_required_extension();

private:
	void _init_application();
	void _vk_create_instance();

	// TODO: move in class
	int _create_logical_device(VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT);
	int _vk_try_setup_debug_callback();

	int _run_main_loop();

	void _init_render_api(bool enableValidationLayers);
	void _init_window();
	void _init_device();
	void _init_swapchain();
	void _init_render();

	VScopedPtr<VkInstance> _instance;
	VScopedPtr<VkDevice> _logical_device;
	VScopedPtr<VkDebugReportCallbackEXT> _callback;
	VkQueue _graphics_queue;
	VkPhysicalDeviceFeatures _device_features{};
	VkSurfaceKHR _surface;

	std::unique_ptr<CG::Renderer> _renderer;
	std::unique_ptr<CG::Window> _window;
	std::unique_ptr<DevicePicker> _picker;
	std::unique_ptr<QueueSelector> _queue_selector;
};

