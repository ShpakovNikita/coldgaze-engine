#include "Core\TriangleEngine.hpp"
#include "Render\Vulkan\Debug.hpp"

CG::TriangleEngine::TriangleEngine(CG::EngineConfig& engineConfig)
    : CG::Engine(engineConfig)
{ }

void CG::TriangleEngine::RenderFrame()
{

}

void CG::TriangleEngine::Prepare()
{
    Engine::Prepare();
    PrepareVertices();
}

void CG::TriangleEngine::PrepareVertices()
{

}
