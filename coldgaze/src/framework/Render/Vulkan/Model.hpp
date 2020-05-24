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

		// Layout structure
		enum class VertexComponent : uint8_t {
			POSITION = 0,
			NORMAL,
			COLOR,
			UV,
			TANGENT,
			BITANGENT,
			DUMMY_FLOAT,
			DUMMY_VEC4,
		};

		struct VertexLayout {
		public:
			/** @brief Components used to generate vertices from */
			std::vector<VertexComponent> components;

			VertexLayout(const std::vector<VertexComponent>& components)
			{
				this->components = components;
			}

			uint32_t GetStride()
			{
				uint32_t res = 0;
				for (auto& component : components)
				{
					switch (component)
					{
					case VertexComponent::UV:
						res += 2 * sizeof(float);
						break;
					case VertexComponent::DUMMY_FLOAT:
						res += sizeof(float);
						break;
					case VertexComponent::DUMMY_VEC4:
						res += 4 * sizeof(float);
						break;
					default:
						// All components except the ones listed above are made up of 3 floats
						res += 3 * sizeof(float);
					}
				}
				return res;
			}
		};

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
				glm::mat4 localMatrix;
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

			~GLTFModel();

			void LoadFromFile(const std::string& filename);

			std::vector<Image>& GetImages();
			std::vector<Material>& GetMaterials();
			const std::vector<Texture>& GetTextures() const;
			const std::vector<Node>& GetNodes() const;

			void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
			void DrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node& node);

		private:
			void LoadTextures(const tinygltf::Model& input);
			void LoadMaterials(const tinygltf::Model& input);
			void LoadImages(const tinygltf::Model& input);

			void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent,
				std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);

			/*
				Model data
			*/
			std::vector<Image> images;
			std::vector<Texture> textures;
			std::vector<Material> materials;
			std::vector<Node> nodes;
		};
	}
}