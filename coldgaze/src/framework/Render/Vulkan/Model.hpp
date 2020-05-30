#pragma once

#include <vector>
#include "glm/vec4.hpp"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/mat4x4.hpp"
#include <string>
#include "vulkan/vulkan_core.h"
#include <memory>
#include "Buffer.hpp"
#include "Texture2D.hpp"

namespace std { class mutex; }

namespace tinygltf 
{ 
	class Node; 
	class Model;
}

namespace CG
{
	namespace Vk
	{
		class Device;
		class Texture2D;

		// TODO: create layout descriptor struct and determine organization by it
		class GLTFModel
		{
		public:
			Device* vkDevice = nullptr;
			VkQueue queue;

			// The vertex layout for the samples' model
			struct Vertex {
				glm::vec3 pos;
				glm::vec3 normal;
				glm::vec2 uv;
				glm::vec3 color;
			};

			// Single vertex buffer for all primitives
			Buffer vertices = {};

			// Single index buffer for all primitives
			struct {
				int count;
				Buffer buffer;
			} indices = {};

			struct Node;

			// A primitive contains the data for a single draw call
			struct Primitive {
				uint32_t firstIndex = 0;
				uint32_t indexCount = 0;
				int32_t materialIndex = 0;
			};

			struct Mesh {
				std::vector<Primitive> primitives;
			};

			struct Node {
				Node* parent = nullptr;
				std::vector<Node> children;
				Mesh mesh;
				glm::mat4 matrix;
			};

			struct Material {
				glm::vec4 baseColorFactor = glm::vec4(1.0f);
				uint32_t baseColorTextureIndex = 0;
				uint32_t normalMapTextureIndex = 0;
				uint32_t metallicRoughnessTextureIndex = 0;

                // We also store (and create) a descriptor set that's used to access this texture from the fragment shader
                VkDescriptorSet descriptorSet = {};
			};

			struct Image {
			    std::unique_ptr<Texture2D> texture;
			};

			struct Texture {
				int32_t imageIndex;
			};

			GLTFModel();
			~GLTFModel();

			void LoadFromFile(const std::string& filename);

			std::vector<Image>& GetImages();
			std::vector<Material>& GetMaterials();
			const std::vector<Texture>& GetTextures() const;
			const std::vector<Node>& GetNodes() const;

			void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
			void DrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node& node);

			bool IsLoaded() const;
			void SetLoaded(bool loaded);

		private:
			void LoadTextures(const tinygltf::Model& input);
			void LoadMaterials(const tinygltf::Model& input);
			void LoadImages(const tinygltf::Model& input);

			void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent,
				std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);

			// This is needed for mising textures cases. It is better to have different shaders per node
			int32_t CreateColorTexture(const glm::vec4& color);
			void CreateSupportingTextures();

			int32_t defaultNormalMapIndex = -1;

			/*
				Model data
			*/
			std::vector<Image> images;
			std::vector<Texture> textures;
			std::vector<Material> materials;
			std::vector<Node> nodes;

			// for async loading, TODO: move mutex here
			bool loaded = false;
		};
	}
}