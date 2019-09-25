#pragma once
#include "Forwards.hpp"
#include "RenderModule/VulkanApi/Renderer.h"
#include "VScopedPtr.hpp"

struct SDL_Window;

namespace CG
{
	class Window
	{
	public:
		Window(const VScopedPtr<VkInstance>& instance);
		~Window();

		bool is_window_alive();
		void poll_events();
		void terminate();

		int32_t show_message_box(const std::string& title, const std::string& message);
		eAssertResponse show_assert_box(const char* stacktrace);

		// TODO: remove this param, make something smarter. Maybe because of specific to vulkan definition remove this from renderer
		VkSurfaceKHR create_surface(eRenderApi renderApi);

		int GetHeight();
		int GetWidth();

	private:
		int _init_window();

		bool _is_window_alive = true;
		SDL_Window* _window = nullptr;
		VkInstance _instance;
		VScopedPtr<VkSurfaceKHR> _surface;

		int _width = 0;
		int _height = 0;
	};
}

