#include "Render/Vulkan/Model.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#include "tinygltf/tiny_gltf.h"
#include <glm/gtc/type_ptr.hpp>
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Debug.hpp"

CG::Vk::GLTFModel::~GLTFModel()
{
	vertices.Destroy();
	indices.buffer.Destroy();
	indices.count = 0;
}

void CG::Vk::GLTFModel::LoadFromFile(const std::string& filename)
{
	tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

	std::vector<uint32_t> indexBuffer;
	std::vector<Vertex> vertexBuffer;

	if (fileLoaded) {
		LoadImages(glTFInput);
		LoadMaterials(glTFInput);
		LoadTextures(glTFInput);
		const tinygltf::Scene& scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			LoadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
		}
	}
	else {
		throw std::runtime_error("Could not open the glTF file. Check, if it is correct");
		return;
	}

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
	indices.count = static_cast<uint32_t>(indexBuffer.size());

	// We are creating this buffers to copy them on local memory for better performance
	Buffer vertexStaging, indexStaging;

	VK_CHECK_RESULT(vkDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&vertexStaging,
		vertexBufferSize,
		vertexBuffer.data()));

	VK_CHECK_RESULT(vkDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&indexStaging,
		indexBufferSize,
		indexBuffer.data()));

	VK_CHECK_RESULT(vkDevice->CreateBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&vertices,
		vertexBufferSize));
	VK_CHECK_RESULT(vkDevice->CreateBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&indices.buffer,
		indexBufferSize));

	VkCommandBuffer copyCmd = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		vertexStaging.buffer,
		vertices.buffer,
		1,
		&copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		indexStaging.buffer,
		indices.buffer.buffer,
		1,
		&copyRegion);

	vkDevice->FlushCommandBuffer(copyCmd, queue, true);

	vertexStaging.Destroy();
	indexStaging.Destroy();
}

void CG::Vk::GLTFModel::LoadTextures(const tinygltf::Model& input)
{
	textures.resize(input.textures.size());
	for (size_t i = 0; i < input.textures.size(); i++) {
		textures[i].imageIndex = input.textures[i].source;
	}
}

void CG::Vk::GLTFModel::LoadMaterials(const tinygltf::Model& input)
{
	materials.resize(input.materials.size());
	for (size_t i = 0; i < input.materials.size(); i++) {

		tinygltf::Material glTFMaterial = input.materials[i];

		if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
			materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
		}

		if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
			materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
		}
	}
}

void CG::Vk::GLTFModel::LoadImages(const tinygltf::Model& input)
{
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		const tinygltf::Image& glTFImage = input.images[i];
		VkDeviceSize bufferSize = 0;

		// We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
		if (glTFImage.component == 3) {
			uint64_t pixelsCount = static_cast<uint64_t>(glTFImage.width) * static_cast<uint64_t>(glTFImage.height);
			bufferSize = pixelsCount * 4;
			unsigned char* buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			const unsigned char* rgb = &glTFImage.image[0];
			for (size_t pixel = 0; pixel < pixelsCount; ++pixel) {
				for (int32_t j = 0; j < 3; ++j) {
					rgba[j] = rgb[j];
				}
				rgba[3] = 0;

				rgba += 4;
				rgb += 3;
			}

			images[i].texture->FromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vkDevice, queue);
		}
		else {
			const unsigned char* buffer = &glTFImage.image[0];
			bufferSize = glTFImage.image.size();

			images[i].texture->FromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vkDevice, queue);
		}
	}
}

void CG::Vk::GLTFModel::LoadNode(
	[[ maybe_unused]] const tinygltf::Node& inputNode,
	[[ maybe_unused]] const tinygltf::Model& input,
	[[ maybe_unused]] Node* parent,
	[[ maybe_unused]] std::vector<uint32_t>& indexBuffer,
	[[ maybe_unused]] std::vector<Vertex>& vertexBuffer)
{

}
