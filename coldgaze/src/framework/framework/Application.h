#pragma once

#include "Forwards.hpp"
#include "VScopedPtr.hpp"

// TODO: think about good error delivery, get rid of try catch
class Application
{
public:
	Application();
	~Application();

	int run();
	static std::vector<VkExtensionProperties> get_available_extensions();
	static bool check_validation_layer_support();

private:
	int _init_window();
	int _init_vulkan();
	int _main_loop();

	VScopedPtr<VkInstance> _instance;
	GLFWwindow* _window = nullptr;

	int _width = 0;
	int _height = 0;
};

