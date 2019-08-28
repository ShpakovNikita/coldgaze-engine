#include "Window.h"
#include "vulkan\vulkan_core.h"

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
#ifdef _WIN32
		VkWin32SurfaceCreateInfoKHR create_info;
		create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		create_info.hwnd = glfwGetWin32Window(_window);
		create_info.hinstance = GetModuleHandle(nullptr);

		auto CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(_instance, "vkCreateWin32SurfaceKHR");

		if (!CreateWin32SurfaceKHR || CreateWin32SurfaceKHR(_instance, &create_info, nullptr, _surface.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}

		if (glfwCreateWindowSurface(_instance, _window, nullptr, _surface.replace()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
#else
		throw std::runtime_error("non win32 system currently not supported");
#endif 
		break;
	}
	default:
		break;
	}

	return _surface;
}
