#include <iostream>
#include <memory>
#include <conio.h>

#include "SDL2/SDL.h"
#include "core/engine.hpp"
#include "core/engine_config.hpp"

int main(int argc, char* argv[]) {
    CG::engine_config engine_config = { 1280, 720 };
	CG::engine engine = { engine_config };

	int exec_result;
	try
	{
		engine.run();
		exec_result = EXIT_SUCCESS;
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		exec_result = EXIT_FAILURE;
	}

	_getch();

	return exec_result;
}
