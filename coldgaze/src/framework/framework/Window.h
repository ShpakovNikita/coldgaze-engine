#pragma once
#include "Forwards.hpp"
#include "Renderer.h"

namespace CG
{
	class Window
	{
	public:
		Window();
		~Window();

		bool is_window_alive();
		void poll_events();
		void terminate();

		// TODO: remove this param, make something smarter. Maybe because of specific to vulkan definition remove this from renderer
		int create_surface(eRenderApi renderApi);

	private:
		int _init_window();

		GLFWwindow* _window = nullptr;

		int _width = 0;
		int _height = 0;
	};
}

