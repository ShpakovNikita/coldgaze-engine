#pragma once

enum class eRenderApi
{
	none = 0,
	vulkan, 
	size,
};

class Renderer
{
public:
	Renderer();
	~Renderer();
};

