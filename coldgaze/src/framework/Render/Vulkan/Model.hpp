#pragma once

#include <vector>
#include "glm/vec4.hpp"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "glm/mat4x4.hpp"

namespace tinygltf { class Model; }

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
			VkQueue copyQueue;

			// The vertex layout for the samples' model
			struct Vertex {
				glm::vec3 pos;
				glm::vec3 normal;
				glm::vec2 uv;
				glm::vec3 color;
			};

			// Single vertex buffer for all primitives
			struct {
				VkBuffer buffer;
				VkDeviceMemory memory;
			} vertices;

			// Single index buffer for all primitives
			struct {
				int count;
				VkBuffer buffer;
				VkDeviceMemory memory;
			} indices;

			struct Node;

			// A primitive contains the data for a single draw call
			struct Primitive {
				uint32_t firstIndex;
				uint32_t indexCount;
				int32_t materialIndex;
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
				uint32_t baseColorTextureIndex;
			};

			struct Image {
			    std::unique_ptr<Texture2D> texture;
				// We also store (and create) a descriptor set that's used to access this texture from the fragment shader
				VkDescriptorSet descriptorSet;
			};

			struct Texture {
				int32_t imageIndex;
			};

			/*
				Model data
			*/
			std::vector<Image> images;
			std::vector<Texture> textures;
			std::vector<Material> materials;
			std::vector<Node> nodes;

			~GLTFModel();

			void loadTextures(const tinygltf::Model& input);
			void loadMaterials(const tinygltf::Model& input);
			void loadImages(const tinygltf::Model& input);
		};
	}
}