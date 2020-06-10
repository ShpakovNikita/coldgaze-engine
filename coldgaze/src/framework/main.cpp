#include <conio.h>
#include <iostream>
#include <memory>

#include "Core/EngineConfig.hpp"
#include "Core/EngineImpl.hpp"
#include "SDL2/SDL.h"

int main(int argc, char* argv[])
{

#if WIN32
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    CG::EngineConfig engine_config = { 1280, 720 };
    for (size_t i = 0; i < argc; i++) {
        engine_config.args.push_back(argv[i]);
    };

    CG::EngineImpl engine = { engine_config };

    int exec_result;
    try {
        engine.Run();
        exec_result = EXIT_SUCCESS;
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        exec_result = EXIT_FAILURE;
    }
    return exec_result;
}
