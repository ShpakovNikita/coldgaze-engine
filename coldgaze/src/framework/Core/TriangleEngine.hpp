#include "engine.hpp"
#include "vulkan/vulkan_core.h"

namespace CG { struct EngineConfig; }

namespace CG
{
    class TriangleEngine : public Engine
    {
    public:
        TriangleEngine(CG::EngineConfig& engineConfig);

    protected:
        void RenderFrame() override;
        void Prepare() override;

    private:
        void PrepareVertices();
    };
}
