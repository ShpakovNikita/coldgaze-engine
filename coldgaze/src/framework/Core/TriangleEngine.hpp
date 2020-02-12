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
		uint32_t CG::TriangleEngine::GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties);

		// Vertex buffer and attributes
		struct {
			VkDeviceMemory memory;	 // Handle to the device memory for this buffer
			VkBuffer buffer;		 // Handle to the Vulkan buffer object that the memory is bound to
		} vertices = {};

		// Index buffer
		struct
		{
			VkDeviceMemory memory;
			VkBuffer buffer;
			uint32_t count;
		} indices = {};
    };
}
