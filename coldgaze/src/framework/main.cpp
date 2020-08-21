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

    CG::EngineConfig engineConfig = { 1280, 720 };
    for (size_t i = 0; i < argc; i++) {
        engineConfig.args.push_back(argv[i]);
    };

    CG::EngineImpl engine = { engineConfig };

    int execResult;
    try {
        engine.Run();
        execResult = EXIT_SUCCESS;
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        execResult = EXIT_FAILURE;
    }
    return execResult;
}
