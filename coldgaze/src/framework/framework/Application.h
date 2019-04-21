#pragma once

#include "Forwards.hpp"
#include "VScopedPtr.hpp"

class DevicePicker;

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
	int _try_setup_debug_callback();

	int _main_loop();

	VScopedPtr<VkInstance> _instance;
	VScopedPtr<VkDebugReportCallbackEXT> _callback;
	GLFWwindow* _window = nullptr;

	std::unique_ptr<DevicePicker> _picker;

	int _width = 0;
	int _height = 0;
};

