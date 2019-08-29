#include <iostream>
#include <memory>
#include <conio.h>

#include "SDL2/SDL.h"
#include "SystemCore/Application.h"

int main(int argc, char* argv[]) {
	std::unique_ptr<Application> app = std::make_unique<Application>();
	
	int exec_result;
	try
	{
		exec_result = app->run();
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		exec_result = EXIT_FAILURE;
	}

	_getch();

	return exec_result;
}
