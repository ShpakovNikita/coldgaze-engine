#include "Render/Vulkan/Model.hpp"
#include "tinygltf/tiny_gltf.h"
#include <glm/gtc/type_ptr.hpp>
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Texture2D.hpp"

CG::Vk::GLTFModel::~GLTFModel()
{
	vkDestroyBuffer(vkDevice->logicalDevice, vertices.buffer, nullptr);
	vkFreeMemory(vkDevice->logicalDevice, vertices.memory, nullptr);
	vkDestroyBuffer(vkDevice->logicalDevice, indices.buffer, nullptr);
	vkFreeMemory(vkDevice->logicalDevice, indices.memory, nullptr);
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

			images[i].texture->FromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vkDevice, copyQueue);
		}
		else {
			const unsigned char* buffer = &glTFImage.image[0];
			bufferSize = glTFImage.image.size();

			images[i].texture->FromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vkDevice, copyQueue);
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

