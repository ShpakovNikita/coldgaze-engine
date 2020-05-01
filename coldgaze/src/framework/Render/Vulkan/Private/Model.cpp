#include "Render/Vulkan/Model.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#include "tinygltf/tiny_gltf.h"
#include <glm/gtc/type_ptr.hpp>
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Debug.hpp"
#include "glm/ext/matrix_transform.hpp"

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

		images[i].texture = std::make_unique<Texture2D>();

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
	const tinygltf::Node& inputNode,
	const tinygltf::Model& input,
	Node* parent,
	std::vector<uint32_t>& indexBuffer,
	std::vector<Vertex>& vertexBuffer)
{
	Node node = {};
	node.localMatrix = glm::mat4(1.0f);

	// Get the local node matrix
	// It's either made up from translation, rotation, scale or a 4x4 matrix
	if (inputNode.translation.size() == 3) 
	{
		node.localMatrix = glm::translate(node.localMatrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
	}
	if (inputNode.rotation.size() == 4) 
	{
		glm::quat q = glm::make_quat(inputNode.rotation.data());
		node.localMatrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3) 
	{
		node.localMatrix = glm::scale(node.localMatrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
	}
	if (inputNode.matrix.size() == 16) 
	{
		node.localMatrix = glm::make_mat4x4(inputNode.matrix.data());
	};

	// Load node's children 
	if (inputNode.children.size() > 0) 
	{
		for (size_t i = 0; i < inputNode.children.size(); i++) 
		{
			LoadNode(input.nodes[inputNode.children[i]], input, &node, indexBuffer, vertexBuffer);
		}
	}

	// if mesh exist in node
	if (inputNode.mesh > -1)
	{
		const tinygltf::Mesh& mesh = input.meshes[inputNode.mesh];

		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				size_t vertexCount = 0;

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					vert.color = glm::vec3(1.0f);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					uint32_t* buf = new uint32_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
					for (size_t index = 0; index < accessor.count; ++index) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					uint16_t* buf = new uint16_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
					for (size_t index = 0; index < accessor.count; ++index) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					uint8_t* buf = new uint8_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
					for (size_t index = 0; index < accessor.count; ++index) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}

			Primitive primitive = {};
			primitive.firstIndex = firstIndex;
			primitive.indexCount = indexCount;
			primitive.materialIndex = glTFPrimitive.material;
			node.mesh.primitives.push_back(primitive);
		}
	}

	if (parent) 
	{
		parent->children.push_back(node);
	}
	else 
	{
		nodes.push_back(node);
	}
}
