#include "Render/Vulkan/Model.hpp"
#include "vulkan/vulkan_core.h"
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

void CG::Vk::GLTFModel::loadTextures(const tinygltf::Model& input)
{
	textures.resize(input.textures.size());
	for (size_t i = 0; i < input.textures.size(); i++) {
		textures[i].imageIndex = input.textures[i].source;
	}
}

void CG::Vk::GLTFModel::loadMaterials(const tinygltf::Model& input)
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

void CG::Vk::GLTFModel::loadImages(const tinygltf::Model& input)
{
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		const tinygltf::Image& glTFImage = input.images[i];
		VkDeviceSize bufferSize = 0;

		// We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
		if (glTFImage.component == 3) {
			bufferSize = glTFImage.width * glTFImage.height * 4;
			unsigned char* buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			const unsigned char* rgb = &glTFImage.image[0];
			for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
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

