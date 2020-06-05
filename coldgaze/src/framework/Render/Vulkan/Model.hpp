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
#include "Debug.hpp"

#pragma warning(push, 0)        
#include "glm/ext/quaternion_float.hpp"
#pragma warning(pop)

namespace std { class mutex; }

namespace tinygltf 
{ 
	class Node; 
	class Model;
	struct Image;
}

// This value also hard coded inside mesh.vert/frag
uint32_t constexpr kMaxJointsCount = 128;

namespace CG
{
	namespace Vk
	{
		class Device;
		class Texture2D;

		// The most of the classes code was took from https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/base/VulkanglTFModel.hpp
		class GLTFModel
		{
		public:
			struct AABBox
			{
				glm::vec3 min = {};
				glm::vec3 max = {};
                bool valid = false;
				AABBox() {};
				AABBox(glm::vec3 min, glm::vec3 max) : min(min), max(max), valid(true){};

				AABBox GetAABB(const glm::mat4& m);
			};

			// TODO: animations update
            // The vertex layout for the model
            struct Vertex
            {
                glm::vec3 pos;
                glm::vec3 normal;
                glm::vec2 uv0;
                glm::vec2 uv1;
                glm::vec4 joint0;
                glm::vec4 weight0;
            };

			Device* vkDevice = nullptr;
			VkQueue queue;

            struct Texture {
				void FromGLTFImage(const tinygltf::Image& gltfimage, TextureSampler textureSampler, Device* device, VkQueue copyQueue);

                Texture2D texture;
            };

            struct Material {
                enum class eAlphaMode
				{ 
					kAlphaModeOpaque,
					kAlphaModeMask, 
					kAlphaModeBlend,
				};
				eAlphaMode alphaMode = eAlphaMode::kAlphaModeOpaque;
                float alphaCutoff = 1.0f;
                float metallicFactor = 1.0f;
                float roughnessFactor = 1.0f;
                glm::vec4 baseColorFactor = glm::vec4(1.0f);
                glm::vec4 emissiveFactor = glm::vec4(1.0f);
                Texture* baseColorTexture = nullptr;
                Texture* metallicRoughnessTexture = nullptr;
                Texture* normalTexture = nullptr;
                Texture* occlusionTexture = nullptr;
                Texture* emissiveTexture = nullptr;
                struct TexCoordSets {
					int baseColor = 0;
					int metallicRoughness = 0;
					int specularGlossiness = 0;
					int normal = 0;
					int occlusion = 0;
					int emissive = 0;
                } texCoordSets;
                struct PbrWorkflows {
                    bool metallicRoughness = true;

					// Not supported for now
                    bool specularGlossiness = false;
                } pbrWorkflows;
                VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            };

			// A primitive contains the data for a single draw call
			struct Primitive {
				uint32_t firstIndex = 0;
				uint32_t firstVertex = 0;
				uint32_t indexCount = 0;
				uint32_t vertexCount = 0;

                // Single vertex buffer for all primitives
				Buffer vertices = {};

                // Single index buffer for all primitives
                Buffer indices = {};

				Material &material;
				bool hasIndices;

				AABBox bbox;

                Primitive(
					uint32_t firstIndex,
					uint32_t firstVertex,
                    uint32_t indexCount,
                    uint32_t vertexCount, 
					Material& material) : firstIndex(firstIndex), firstVertex(firstVertex), indexCount(indexCount), vertexCount(vertexCount), material(material) {
                    hasIndices = indexCount > 0;
                };
			};

			struct Mesh {
				std::vector<std::unique_ptr<Primitive>> primitives = {};

				AABBox bbox = {};

                struct UniformBuffer {
					Buffer buffer;
                    VkDescriptorSet descriptorSet;
                } uniformBuffer;

                struct UniformBlock {
                    glm::mat4 matrix;
                    glm::mat4 jointMatrix[kMaxJointsCount];
                    float jointcount = 0;
                } uniformBlock;

				Mesh(Device* vkDevice, const glm::mat4& meshMat);

				void UpdateUniformBuffers();

			};

            struct Node;

            struct Skin {
                std::string name;
                Node* skeletonRoot = nullptr;
                std::vector<glm::mat4> inverseBindMatrices;
                std::vector<Node*> joints;
            };

			struct Node {
                Node* parent = nullptr;
                uint32_t index;
                std::vector<std::unique_ptr<Node>> children;
                glm::mat4 matrix;
                std::string name;
                std::unique_ptr<Mesh> mesh;
                Skin* skin = nullptr;
                int32_t skinIndex = -1;
                glm::vec3 translation {};
                glm::vec3 scale { 1.0f };
                glm::quat rotation {};

				AABBox bbox;

				glm::mat4 GetLocalMatrix();
				glm::mat4 GetWorldMatrix();

				void UpdateRecursive();
			};

			struct AnimationChannel {
				enum ePathType
				{ 
					kTranslation, 
					kRotation, 
					kScale,
				};
				ePathType path;
				Node* node;
				uint32_t samplerIndex;
			};

			struct AnimationSampler {
				enum eInterpolationType 
				{ 
					kLinear, 
					kStep, 
					kCubicSpline,
				};
				eInterpolationType interpolation;
				std::vector<float> inputs;
				std::vector<glm::vec4> outputsVec4;
			};

			struct Animation {
				std::string name;
				std::vector<AnimationSampler> samplers;
				std::vector<AnimationChannel> channels;
				float start = std::numeric_limits<float>::max();
				float end = std::numeric_limits<float>::min();
			};

			GLTFModel();
			~GLTFModel();

			void LoadFromFile(const std::string& filename, float scale = 1.0f);

			std::vector<Material>& GetMaterials();
			const std::vector<Texture>& GetTextures() const;
			const std::vector<std::unique_ptr<Node>>& GetNodes() const;
			const std::vector<Node*>& GetFlatNodes() const;

			void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
			void DrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, const Node& node);

			bool IsLoaded() const;
			void SetLoaded(bool loaded);

			const glm::vec3& GetSize() const;
			const uint32_t GetPrimitivesCount();

		private:
			static VkSamplerAddressMode GetVkWrapMode(int32_t wrapMode);
			static VkFilter GetVkFilterMode(int32_t filterMode);

			void LoadTextureSamplers(const tinygltf::Model& input);
			void LoadTextures(const tinygltf::Model& input);
            void LoadMaterials(const tinygltf::Model& input);
			void LoadAnimations(const tinygltf::Model& input);
            void LoadSkins(const tinygltf::Model& input);

			void CalculateSize();

			void CreatePrimitiveBuffers(Primitive* newPrimitive, std::vector<Vertex>& vertexBuffer, 
				std::vector<uint32_t>& indexBuffer);

			void LoadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& input,
				float globalscale);

			/*
				Model data
			*/
            std::vector<std::unique_ptr<Node>> nodes;
            std::vector<Node*> allNodes;

            std::vector<std::unique_ptr<Skin>> skins;

            std::vector<Texture> textures;
            std::vector<TextureSampler> textureSamplers;
            std::vector<Material> materials;
            std::vector<Animation> animations;
            std::vector<std::string> extensions;

			glm::vec3 size = {};

			// for async loading, TODO: move mutex here
			bool loaded = false;
		};
	}
}