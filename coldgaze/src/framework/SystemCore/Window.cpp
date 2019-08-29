#include "Window.h"
#include "vulkan\vulkan.h"

using namespace CG;

namespace SWindow
{
	static const int width = 800;
	static const int height = 600;
}

Window::Window(const VScopedPtr<VkInstance>& instance)
	: _instance(instance)
	, _surface(instance, vkDestroySurfaceKHR)
{
	_init_window();
}

Window::~Window()
{
	if (_window != nullptr)
	{
		terminate();
	}
}

bool Window::is_window_alive()
{
	return !glfwWindowShouldClose(_window);
}

void Window::poll_events()
{
	glfwPollEvents();
}

void Window::terminate()
{
	glfwDestroyWindow(_window);

	glfwTerminate();
}

int Window::_init_window()
{
	using namespace SWindow;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	_width = width;
	_height = height;

	_window = glfwCreateWindow(_width, _height, "CG window", nullptr, nullptr);

	return CG_INIT_SUCCESS;
}

VkSurfaceKHR CG::Window::create_surface(eRenderApi renderApi)
{
	switch (renderApi)
	{
	case eRenderApi::none:
		break;

	case eRenderApi::vulkan:
	{
		if (glfwCreateWindowSurface(_instance, _window, nullptr, _surface.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
		break;
	}
	default:
		break;
	}

	return _surface;
}
