#include "Window.h"

using namespace CG;

namespace SWindow
{
	static const int width = 800;
	static const int height = 600;
}

Window::Window()
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

int CG::Window::create_surface(eRenderApi renderApi)
{
	int result = -1;

	switch (renderApi)
	{
	case eRenderApi::none:
		break;

	case eRenderApi::vulkan:
#ifdef _WIN32
		VkWin32SurfaceCreateInfoKHR create_info;
		create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		create_info.hwnd = glfwGetWin32Window(_window);
		create_info.hinstance = GetModuleHandle(nullptr);
		result = CG_INIT_SUCCESS;
#else
		throw std::runtime_error("non win32 system currently not supported");
		result = GLFW_PLATFORM_ERROR;
#endif 
		break;

	default:
		break;
	}

	return result;
}
