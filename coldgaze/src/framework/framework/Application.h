#pragma once
class Application
{
public:
	Application();
	~Application();

	int run();

private:
	void _init_vulkan();
	void _main_loop();
};

