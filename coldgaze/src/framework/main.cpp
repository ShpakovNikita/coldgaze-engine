#include <iostream>
#include <memory>
#include <conio.h>

#include "SDL2/SDL.h"
#include "Core/Engine.hpp"
#include "Core/EngineConfig.hpp"

int main(int argc, char* argv[]) {
    CG::EngineConfig engine_config = { 1280, 720 };
	for (size_t i = 0; i < argc; i++) { engine_config.args.push_back(argv[i]); };

	CG::Engine engine = { engine_config };

	int exec_result;
	try
	{
		engine.Run();
		exec_result = EXIT_SUCCESS;
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		exec_result = EXIT_FAILURE;
	}
	return exec_result;
}
