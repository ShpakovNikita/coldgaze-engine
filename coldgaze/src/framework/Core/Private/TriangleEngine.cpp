#include "Core\TriangleEngine.hpp"
#include "Render\Vulkan\Debug.hpp"
#include "Render\Vulkan\Device.hpp"
#include "Core\EngineConfig.hpp"

#include <glm/gtc/matrix_transform.hpp>

constexpr uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;
constexpr float DEFAULT_FOV = 60.0f;

namespace STriangleEngine
{
	struct Vertex {
		float position[3];
		float color[3];
	};
}

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
	PrepareUniformBuffers();
	SetupDescriptorSetLayout();
	PreparePipelines();
}

uint32_t CG::TriangleEngine::GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
{
	// Iterate over all memory types available for the device used
	for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		typeBits >>= 1;
	}

	throw std::runtime_error("Could not find a suitable memory type!");
}

VkCommandBuffer CG::TriangleEngine::GetReadyCommandBuffer()
{
	VkDevice device = vkDevice->logicalDevice;
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = vkCmdPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

    VkCommandBufferBeginInfo cmdBufferBeginInfo{};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

    return cmdBuffer;
}

void CG::TriangleEngine::FlushCommandBuffer(VkCommandBuffer commandBuffer)
{
	VkDevice device = vkDevice->logicalDevice;

    assert(commandBuffer != VK_NULL_HANDLE);

    VkSubmitInfo queueSubmitInfo = {};
    queueSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    queueSubmitInfo.commandBufferCount = 1;
    queueSubmitInfo.pCommandBuffers = &commandBuffer;

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VkFence fence;
    VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));

    // Submit to the queue
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &queueSubmitInfo, fence));
    // Wait for the fence to signal that command buffer has finished executing
    VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

    VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, vkCmdPool, 1, &commandBuffer);
}

void CG::TriangleEngine::UpdateUniformBuffers()
{
	VkDevice device = vkDevice->logicalDevice;

    uboVS.projectionMatrix = glm::perspective(glm::radians(DEFAULT_FOV), (float)engineConfig.width / (float)engineConfig.height, 0.1f, 256.0f);

    uboVS.viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom));

	// TODO: rotation matrix
    uboVS.modelMatrix = glm::mat4(1.0f);
    uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    uboVS.modelMatrix = glm::rotate(uboVS.modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    uint8_t* pData;
    VK_CHECK_RESULT(vkMapMemory(device, uniformBufferVS.memory, 0, sizeof(uboVS), 0, (void**)&pData));
    memcpy(pData, &uboVS, sizeof(uboVS));

    // Note: Since we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU, that's why I can use unmap here
    vkUnmapMemory(device, uniformBufferVS.memory);
}

void CG::TriangleEngine::PrepareVertices()
{
	using namespace STriangleEngine;

	VkDevice device = vkDevice->logicalDevice;

	std::vector<Vertex> vertexBuffer =
	{
		{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
		{ {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
	};

	uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

	std::vector<uint32_t> indexBuffer = { 0, 1, 2 };
	indices.count = static_cast<uint32_t>(indexBuffer.size());
	uint32_t indexBufferSize = indices.count * sizeof(uint32_t);

	VkMemoryAllocateInfo memAlloc = {};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	void* data;

	struct StagingBuffer {
		VkDeviceMemory memory;
		VkBuffer buffer;
	};

	struct {
		StagingBuffer vertices;
		StagingBuffer indices;
	} stagingBuffers;

	VkBufferCreateInfo vertexBufferInfo = {};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.size = vertexBufferSize;
	// Buffer is used as the copy source
	vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VK_CHECK_RESULT(vkCreateBuffer(device, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer));
	vkGetBufferMemoryRequirements(device, stagingBuffers.vertices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &stagingBuffers.vertices.memory));
	VK_CHECK_RESULT(vkMapMemory(device, stagingBuffers.vertices.memory, 0, memAlloc.allocationSize, 0, &data));
	memcpy(data, vertexBuffer.data(), vertexBufferSize);
	vkUnmapMemory(device, stagingBuffers.vertices.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0));

	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(device, &vertexBufferInfo, nullptr, &vertices.buffer));
	vkGetBufferMemoryRequirements(device, vertices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &vertices.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device, vertices.buffer, vertices.memory, 0));

	VkBufferCreateInfo indexbufferInfo = {};
	indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexbufferInfo.size = indexBufferSize;
	indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(device, &indexbufferInfo, nullptr, &stagingBuffers.indices.buffer));
	vkGetBufferMemoryRequirements(device, stagingBuffers.indices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &stagingBuffers.indices.memory));
	VK_CHECK_RESULT(vkMapMemory(device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data));
	memcpy(data, indexBuffer.data(), indexBufferSize);
	vkUnmapMemory(device, stagingBuffers.indices.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0));

	indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(device, &indexbufferInfo, nullptr, &indices.buffer));
	vkGetBufferMemoryRequirements(device, indices.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &indices.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device, indices.buffer, indices.memory, 0));

	VkCommandBuffer copyCmd = GetReadyCommandBuffer();

	// start using command buffers
	VkCommandBufferBeginInfo cmdBufferBeginInfo{};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufferBeginInfo));

	// Vertex buffer
	VkBufferCopy copyRegion = {};
	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffers.vertices.buffer, vertices.buffer, 1, &copyRegion);
	// Index buffer
	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffers.indices.buffer, indices.buffer, 1, &copyRegion);

	FlushCommandBuffer(copyCmd);

	vkDestroyBuffer(device, stagingBuffers.vertices.buffer, nullptr);
	vkFreeMemory(device, stagingBuffers.vertices.memory, nullptr);
	vkDestroyBuffer(device, stagingBuffers.indices.buffer, nullptr);
	vkFreeMemory(device, stagingBuffers.indices.memory, nullptr);
}

void CG::TriangleEngine::PrepareUniformBuffers()
{
	VkDevice device = vkDevice->logicalDevice;

    VkMemoryRequirements memReqs;

    VkBufferCreateInfo bufferInfo = {};
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.allocationSize = 0;
    allocInfo.memoryTypeIndex = 0;

    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(uboVS);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBufferVS.buffer));
    vkGetBufferMemoryRequirements(device, uniformBufferVS.buffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &(uniformBufferVS.memory)));
    VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBufferVS.buffer, uniformBufferVS.memory, 0));

    // Store information in the uniform's descriptor that is used by the descriptor set
    uniformBufferVS.descriptor.buffer = uniformBufferVS.buffer;
    uniformBufferVS.descriptor.offset = 0;
    uniformBufferVS.descriptor.range = sizeof(uboVS);

	UpdateUniformBuffers();
}

void CG::TriangleEngine::SetupDescriptorSetLayout()
{
	VkDevice device = vkDevice->logicalDevice;

	// Binding 0: Uniform buffer (Vertex shader)
	VkDescriptorSetLayoutBinding layoutBinding = {};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
	descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pNext = nullptr;
	descriptorLayout.bindingCount = 1;
	descriptorLayout.pBindings = &layoutBinding;

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
	pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pPipelineLayoutCreateInfo.pNext = nullptr;
	pPipelineLayoutCreateInfo.setLayoutCount = 1;
	pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));

}

void CG::TriangleEngine::PreparePipelines()
{

}
