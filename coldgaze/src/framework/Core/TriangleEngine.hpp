#include "engine.hpp"
#include <glm/glm.hpp>
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
		VkCommandBuffer GetReadyCommandBuffer();
		void FlushCommandBuffer(VkCommandBuffer commandBuffer);

        // TODO: move to camera or something like that
        void UpdateUniformBuffers();

        void PrepareVertices();
		void PrepareUniformBuffers();
        void SetupDescriptorSetLayout();
		void PreparePipelines();

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

        struct {
            VkDeviceMemory memory;
            VkBuffer buffer;
            VkDescriptorBufferInfo descriptor;
        }  uniformBufferVS = {};

		VkPipelineLayout pipelineLayout = {};
		VkDescriptorSetLayout descriptorSetLayout = {};

        // TODO: move to camera or something like that
        struct {
            glm::mat4 projectionMatrix;
            glm::mat4 modelMatrix;
            glm::mat4 viewMatrix;
		} uboVS = {};

        glm::vec3 rotation = glm::vec3();
        glm::vec3 cameraPos = glm::vec3();

        float zoom = 0;
    };
}
