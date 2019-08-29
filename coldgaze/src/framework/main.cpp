#include <iostream>

#include <memory>
#include "SystemCore/Application.h"
#include <conio.h>

int main() {
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
