#pragma once
#include "Forwards.hpp"

enum class eRenderApi
{
	none = 0,
	vulkan, 
	size,
};

namespace CG
{
	class Renderer
	{
	public:
		Renderer(VkSurfaceKHR surface);
		~Renderer();

	private:
		VkSurfaceKHR _surface;
	};
}
