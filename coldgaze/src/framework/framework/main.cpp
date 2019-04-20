#include <iostream>

#include <memory>
#include "Application.h"

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
		return EXIT_FAILURE;
	}

	return exec_result;
}
