#include "Render/Vulkan/Model.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT

#pragma warning(push, 0)
#include "glm/ext/matrix_transform.hpp"
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include "Render/Vulkan/Debug.hpp"
#include "Render/Vulkan/Device.hpp"
#include "Render/Vulkan/Exceptions.hpp"
#include "glm/common.hpp"
#include "tinygltf/tiny_gltf.h"
#include <mutex>

namespace SGLTFModel {
CG::Vk::GLTFModel::Node* FindNode(CG::Vk::GLTFModel::Node* parent, uint32_t index)
{
    CG::Vk::GLTFModel::Node* nodeFound = nullptr;
    if (parent->index == index) {
        return parent;
    }
    for (auto& child : parent->children) {
        nodeFound = FindNode(child.get(), index);
        if (nodeFound) {
            break;
        }
    }
    return nodeFound;
}

CG::Vk::GLTFModel::Node* NodeFromIndex(uint32_t index, const std::vector<std::unique_ptr<CG::Vk::GLTFModel::Node>>& nodes)
{
    CG::Vk::GLTFModel::Node* nodeFound = nullptr;
    for (auto& node : nodes) {
        nodeFound = FindNode(node.get(), index);
        if (nodeFound) {
            break;
        }
    }
    return nodeFound;
}

template <typename T>
void FillVertexAttribute(const tinygltf::Primitive& primitive, const tinygltf::Model& input, const std::string& attrName, int compSize, const T** buffer, int& bufferStride)
{
    if (primitive.attributes.find(attrName) != primitive.attributes.end()) {
        const tinygltf::Accessor& accessor = input.accessors[primitive.attributes.find(attrName)->second];
        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
        *buffer = reinterpret_cast<const T*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
        bufferStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(T)) : tinygltf::GetNumComponentsInType(compSize);
    }
}
}

CG::Vk::GLTFModel::GLTFModel()
{
}

CG::Vk::GLTFModel::~GLTFModel()
{
    loaded = false;

    for (auto& texture : textures) {
        texture.texture.Destroy();
    }

    for (auto& material : materials) {
        material->materialParams.Destroy();
    }

    for (auto& node : allNodes) {
        if (node->mesh) {
            for (auto& primitive : node->mesh->primitives) {
                primitive->vertices.Destroy();
                primitive->indices.Destroy();
            }
        }
    }
}

void CG::Vk::GLTFModel::LoadFromFile(const std::string& filename, float scale /*= 1.0f*/)
{
    tinygltf::Model glTFInput;
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

    bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    if (fileLoaded) {
        LoadTextureSamplers(glTFInput);
        LoadTextures(glTFInput);
        LoadMaterials(glTFInput);

        if (glTFInput.scenes.empty()) {
            throw AssetLoadingException("Could not the load file!");
        }

        const tinygltf::Scene& scene = glTFInput.scenes[glTFInput.defaultScene > -1 ? glTFInput.defaultScene : 0];
        for (size_t i = 0; i < scene.nodes.size(); i++) {
            const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
            LoadNode(nullptr, node, scene.nodes[i], glTFInput, scale);
        }

        LoadAnimations(glTFInput);
        LoadSkins(glTFInput);

        for (auto node : allNodes) {
            // Assign skins
            if (node->skinIndex > -1) {
                node->skin = skins[node->skinIndex].get();
            }
            // Initial pose
            if (node->mesh) {
                node->UpdateRecursive();
            }
        }

        CalculateSize();
    } else {
        throw AssetLoadingException("Could not open the glTF file. Check, if it is correct");
        return;
    }

    extensions = glTFInput.extensionsUsed;
}

std::vector<std::unique_ptr<CG::Vk::GLTFModel::Material>>& CG::Vk::GLTFModel::GetMaterials()
{
    return materials;
}

const std::vector<CG::Vk::GLTFModel::Texture>& CG::Vk::GLTFModel::GetTextures() const
{
    return textures;
}

const std::vector<std::unique_ptr<CG::Vk::GLTFModel::Node>>& CG::Vk::GLTFModel::GetNodes() const
{
    return nodes;
}

const std::vector<CG::Vk::GLTFModel::Node*>& CG::Vk::GLTFModel::GetFlatNodes() const
{
    return allNodes;
}

bool CG::Vk::GLTFModel::IsLoaded() const
{
    return loaded;
}

void CG::Vk::GLTFModel::SetLoaded(bool aLoaded)
{
    loaded = aLoaded;
}

const glm::vec3& CG::Vk::GLTFModel::GetSize() const
{
    return size;
}

const uint32_t CG::Vk::GLTFModel::GetPrimitivesCount()
{
    uint32_t primCount = 0;
    for (const auto& node : allNodes) {
        if (node->mesh) {
            primCount += static_cast<uint32_t>(node->mesh->primitives.size());
        }
    }

    return primCount;
}

// from GLTF2 specs
VkSamplerAddressMode CG::Vk::GLTFModel::GetVkWrapMode(int32_t wrapMode)
{
    switch (wrapMode) {
    case 10497:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case 33071:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case 33648:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

// from GLTF2 specs
VkFilter CG::Vk::GLTFModel::GetVkFilterMode(int32_t filterMode)
{
    switch (filterMode) {
    case 9728:
        return VK_FILTER_NEAREST;
    case 9729:
        return VK_FILTER_LINEAR;
    case 9984:
        return VK_FILTER_NEAREST;
    case 9985:
        return VK_FILTER_NEAREST;
    case 9986:
        return VK_FILTER_LINEAR;
    case 9987:
        return VK_FILTER_LINEAR;
    default:
        return VK_FILTER_NEAREST;
    }
}

void CG::Vk::GLTFModel::LoadTextureSamplers(const tinygltf::Model& input)
{
    for (tinygltf::Sampler smpl : input.samplers) {
        TextureSampler sampler;
        sampler.minFilter = GetVkFilterMode(smpl.minFilter);
        sampler.magFilter = GetVkFilterMode(smpl.magFilter);
        sampler.addressModeU = GetVkWrapMode(smpl.wrapS);
        sampler.addressModeV = GetVkWrapMode(smpl.wrapT);
        sampler.addressModeW = sampler.addressModeV;
        textureSamplers.push_back(sampler);
    }
}

void CG::Vk::GLTFModel::LoadTextures(const tinygltf::Model& input)
{
    textures.reserve(input.textures.size());

    for (const tinygltf::Texture& tex : input.textures) {
        const tinygltf::Image& image = input.images[tex.source];
        TextureSampler textureSampler;
        if (tex.sampler == -1) {
            textureSampler.magFilter = VK_FILTER_LINEAR;
            textureSampler.minFilter = VK_FILTER_LINEAR;
            textureSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            textureSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            textureSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        } else {
            textureSampler = textureSamplers[tex.sampler];
        }
        Texture texture;
        texture.FromGLTFImage(image, textureSampler, vkDevice, queue);
        textures.push_back(texture);
    }
}

void CG::Vk::GLTFModel::LoadMaterials(const tinygltf::Model& input)
{
    for (const tinygltf::Material& mat : input.materials) {
        std::unique_ptr<Material> material = std::make_unique<Material>();
        if (mat.values.find("baseColorTexture") != mat.values.end()) {
            material->baseColorTexture = &textures[mat.values.at("baseColorTexture").TextureIndex()];
            material->texCoordSets.baseColor = mat.values.at("baseColorTexture").TextureTexCoord();
        }
        if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
            material->metallicRoughnessTexture = &textures[mat.values.at("metallicRoughnessTexture").TextureIndex()];
            material->texCoordSets.metallicRoughness = mat.values.at("metallicRoughnessTexture").TextureTexCoord();
        }
        if (mat.values.find("roughnessFactor") != mat.values.end()) {
            material->roughnessFactor = static_cast<float>(mat.values.at("roughnessFactor").Factor());
        }
        if (mat.values.find("metallicFactor") != mat.values.end()) {
            material->metallicFactor = static_cast<float>(mat.values.at("metallicFactor").Factor());
        }
        if (mat.values.find("baseColorFactor") != mat.values.end()) {
            material->baseColorFactor = glm::make_vec4(mat.values.at("baseColorFactor").ColorFactor().data());
        }
        if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
            material->normalTexture = &textures[mat.additionalValues.at("normalTexture").TextureIndex()];
            material->texCoordSets.normal = mat.additionalValues.at("normalTexture").TextureTexCoord();
        }
        if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
            material->emissiveTexture = &textures[mat.additionalValues.at("emissiveTexture").TextureIndex()];
            material->texCoordSets.emissive = mat.additionalValues.at("emissiveTexture").TextureTexCoord();
        }
        if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
            material->occlusionTexture = &textures[mat.additionalValues.at("occlusionTexture").TextureIndex()];
            material->texCoordSets.occlusion = mat.additionalValues.at("occlusionTexture").TextureTexCoord();
        }
        if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end()) {
            tinygltf::Parameter param = mat.additionalValues.at("alphaMode");
            if (param.string_value == "BLEND") {
                material->alphaMode = Material::eAlphaMode::kAlphaModeBlend;
            }
            if (param.string_value == "MASK") {
                material->alphaCutoff = 0.5f;
                material->alphaMode = Material::eAlphaMode::kAlphaModeMask;
            }
        }
        if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end()) {
            material->alphaCutoff = static_cast<float>(mat.additionalValues.at("alphaCutoff").Factor());
        }
        if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end()) {
            material->emissiveFactor = glm::vec4(glm::make_vec3(mat.additionalValues.at("emissiveFactor").ColorFactor().data()), 1.0);
            material->emissiveFactor = glm::vec4(0.0f);
        }

        Material::MaterialParams materialParams;

        materialParams.baseColorFactor = material->baseColorFactor;
        materialParams.metallicFactor = material->metallicFactor;
        materialParams.roughnessFactor = material->roughnessFactor;
        materialParams.emissiveFactor = material->emissiveFactor;

        // Notice: this value used in shader
        materialParams.workflow = 0.0f;

        materialParams.colorTextureSet = material->baseColorTexture != nullptr ? material->texCoordSets.baseColor : -1;
        materialParams.physicalDescriptorTextureSet = material->metallicRoughnessTexture != nullptr ? material->texCoordSets.metallicRoughness : -1;
        materialParams.normalTextureSet = material->normalTexture != nullptr ? material->texCoordSets.normal : -1;
        materialParams.occlusionTextureSet = material->occlusionTexture != nullptr ? material->texCoordSets.occlusion : -1;
        materialParams.emissiveTextureSet = material->emissiveTexture != nullptr ? material->texCoordSets.emissive : -1;

        materialParams.baseColorFactor = material->baseColorFactor;
        materialParams.metallicFactor = material->metallicFactor;
        materialParams.roughnessFactor = material->roughnessFactor;
        materialParams.alphaMaskCutoff = material->alphaCutoff;

        size_t materialBufferSize = sizeof(Material::MaterialParams);

        // We are creating this buffers to copy them on local memory for better performance
        Buffer materialStaging;

        VK_CHECK_RESULT(vkDevice->CreateBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &materialStaging,
            materialBufferSize,
            &materialParams));

        VK_CHECK_RESULT(vkDevice->CreateBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &material->materialParams,
            materialBufferSize));

        VkCommandBuffer copyCmd = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy copyRegion = {};

        copyRegion.size = materialBufferSize;
        vkCmdCopyBuffer(
            copyCmd,
            materialStaging.buffer,
            material->materialParams.buffer,
            1,
            &copyRegion);

        vkDevice->FlushCommandBuffer(copyCmd, queue, true);
        materialStaging.Destroy();

        materials.push_back(std::move(material));
    }
    // Push a default material at the end of the list for meshes with no material assigned
    materials.push_back(std::move(std::make_unique<Material>()));
}

void CG::Vk::GLTFModel::LoadAnimations(const tinygltf::Model& input)
{
    for (const tinygltf::Animation& anim : input.animations) {
        Animation animation = {};
        animation.name = anim.name;

        // Samplers
        for (const tinygltf::AnimationSampler& samp : anim.samplers) {
            AnimationSampler sampler = {};

            if (samp.interpolation == "LINEAR") {
                sampler.interpolation = AnimationSampler::eInterpolationType::kLinear;
            }
            if (samp.interpolation == "STEP") {
                sampler.interpolation = AnimationSampler::eInterpolationType::kStep;
            }
            if (samp.interpolation == "CUBICSPLINE") {
                sampler.interpolation = AnimationSampler::eInterpolationType::kCubicSpline;
            }

            // Read sampler input time values
            {
                const tinygltf::Accessor& accessor = input.accessors[samp.input];
                const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                const float* buf = static_cast<const float*>(dataPtr);
                for (size_t index = 0; index < accessor.count; index++) {
                    sampler.inputs.push_back(buf[index]);
                }

                for (auto animInput : sampler.inputs) {
                    if (animInput < animation.start) {
                        animation.start = animInput;
                    };
                    if (animInput > animation.end) {
                        animation.end = animInput;
                    }
                }
            }

            // Read sampler output T/R/S values
            {
                const tinygltf::Accessor& accessor = input.accessors[samp.output];
                const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

                switch (accessor.type) {
                case TINYGLTF_TYPE_VEC3: {
                    const glm::vec3* buf = static_cast<const glm::vec3*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++) {
                        sampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
                    }
                    break;
                }
                case TINYGLTF_TYPE_VEC4: {
                    const glm::vec4* buf = static_cast<const glm::vec4*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++) {
                        sampler.outputsVec4.push_back(buf[index]);
                    }
                    break;
                }
                default: {
                    std::cout << "unknown type" << std::endl;
                    break;
                }
                }
            }

            animation.samplers.push_back(sampler);
        }

        // Channels
        for (const tinygltf::AnimationChannel& source : anim.channels) {
            AnimationChannel channel = {};

            if (source.target_path == "rotation") {
                channel.path = AnimationChannel::ePathType::kRotation;
            }
            if (source.target_path == "translation") {
                channel.path = AnimationChannel::ePathType::kTranslation;
            }
            if (source.target_path == "scale") {
                channel.path = AnimationChannel::ePathType::kScale;
            }
            if (source.target_path == "weights") {
                std::cout << "weights not yet supported, skipping channel" << std::endl;
                continue;
            }
            channel.samplerIndex = source.sampler;
            channel.node = SGLTFModel::NodeFromIndex(source.target_node, nodes);
            if (!channel.node) {
                continue;
            }

            animation.channels.push_back(channel);
        }

        animations.push_back(animation);
    }
}

void CG::Vk::GLTFModel::LoadSkins(const tinygltf::Model& input)
{
    for (const tinygltf::Skin& source : input.skins) {
        std::unique_ptr<Skin> newSkin = std::make_unique<Skin>();
        newSkin->name = source.name;

        // Find skeleton root node
        if (source.skeleton > -1) {
            newSkin->skeletonRoot = SGLTFModel::NodeFromIndex(source.skeleton, nodes);
        }

        // Find joint nodes
        for (int jointIndex : source.joints) {
            Node* node = SGLTFModel::NodeFromIndex(jointIndex, nodes);
            if (node) {
                newSkin->joints.push_back(SGLTFModel::NodeFromIndex(jointIndex, nodes));
            }
        }

        // Get inverse bind matrices from buffer
        if (source.inverseBindMatrices > -1) {
            const tinygltf::Accessor& accessor = input.accessors[source.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];
            newSkin->inverseBindMatrices.resize(accessor.count);
            memcpy(newSkin->inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
        }

        skins.push_back(std::move(newSkin));
    }
}

void CG::Vk::GLTFModel::CalculateSize()
{
    AABBox dimension;

    dimension.min = glm::vec3(FLT_MAX);
    dimension.max = glm::vec3(-FLT_MAX);

    for (const auto& node : allNodes) {
        if (node->mesh && node->mesh->bbox.valid) {
            AABBox bbox = node->mesh->bbox.GetAABB(node->GetWorldMatrix());
            dimension.min = glm::min(dimension.min, bbox.min);
            dimension.max = glm::max(dimension.max, bbox.max);
        }
    }

    size = glm::vec3(dimension.max[0] - dimension.min[0], dimension.max[1] - dimension.min[1], dimension.max[2] - dimension.min[2]);
}

void CG::Vk::GLTFModel::CreatePrimitiveBuffers(Primitive* newPrimitive, std::vector<Vertex>& vertexBuffer,
    std::vector<uint32_t>& indexBuffer)
{
    size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
    size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
    newPrimitive->indexCount = static_cast<uint32_t>(indexBuffer.size());
    newPrimitive->vertexCount = static_cast<uint32_t>(vertexBuffer.size());

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
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &newPrimitive->vertices,
        vertexBufferSize));
    VK_CHECK_RESULT(vkDevice->CreateBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &newPrimitive->indices,
        indexBufferSize));

    VkCommandBuffer copyCmd = vkDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(
        copyCmd,
        vertexStaging.buffer,
        newPrimitive->vertices.buffer,
        1,
        &copyRegion);

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(
        copyCmd,
        indexStaging.buffer,
        newPrimitive->indices.buffer,
        1,
        &copyRegion);

    vkDevice->FlushCommandBuffer(copyCmd, queue, true);

    vertexStaging.Destroy();
    indexStaging.Destroy();
}

void CG::Vk::GLTFModel::LoadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex,
    const tinygltf::Model& input, float globalscale)
{
    std::unique_ptr<Node> newNode = std::make_unique<Node>();
    newNode->index = nodeIndex;
    newNode->parent = parent;
    newNode->name = node.name;
    newNode->skinIndex = node.skin;
    newNode->matrix = glm::mat4(1.0f);

    // Generate local node matrix
    glm::vec3 translation = glm::vec3(0.0f);
    if (node.translation.size() == 3) {
        translation = glm::make_vec3(node.translation.data());
        newNode->translation = translation;
    }
    glm::mat4 rotation = glm::mat4(1.0f);
    if (node.rotation.size() == 4) {
        glm::quat q = glm::make_quat(node.rotation.data());
        newNode->rotation = glm::mat4(q);
    }
    glm::vec3 scale = glm::vec3(1.0f);
    if (node.scale.size() == 3) {
        scale = glm::make_vec3(node.scale.data());
        newNode->scale = scale;
    }
    if (node.matrix.size() == 16) {
        newNode->matrix = glm::make_mat4x4(node.matrix.data());
    };

    // Node with children
    if (node.children.size() > 0) {
        for (size_t i = 0; i < node.children.size(); i++) {
            LoadNode(newNode.get(), input.nodes[node.children[i]], node.children[i], input, globalscale);
        }
    }

    // Node contains mesh data
    if (node.mesh > -1) {
        const tinygltf::Mesh mesh = input.meshes[node.mesh];
        std::unique_ptr<Mesh> newMesh = std::make_unique<Mesh>(vkDevice, newNode->matrix);
        for (size_t j = 0; j < mesh.primitives.size(); j++) {
            const tinygltf::Primitive& primitive = mesh.primitives[j];

            std::vector<uint32_t> indexBuffer;
            indexBuffer.reserve(4096);
            std::vector<Vertex> vertexBuffer;
            vertexBuffer.reserve(4096);

            uint32_t indexStart = 0;
            uint32_t vertexStart = 0;
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            bool hasSkin = false;
            bool hasIndices = primitive.indices > -1;

            glm::vec3 posMin {};
            glm::vec3 posMax {};

            // Vertices
            {
                const float* bufferPos = nullptr;
                const float* bufferNormals = nullptr;
                const float* bufferTexCoordSet0 = nullptr;
                const float* bufferTexCoordSet1 = nullptr;
                const uint16_t* bufferJoints = nullptr;
                const float* bufferWeights = nullptr;

                int posByteStride = 0;
                int normByteStride = 0;
                int uv0ByteStride = 0;
                int uv1ByteStride = 0;
                int jointByteStride = 0;
                int weightByteStride = 0;

                // Position attribute is required
                assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

                const tinygltf::Accessor& posAccessor = input.accessors[primitive.attributes.find("POSITION")->second];
                vertexCount = static_cast<uint32_t>(posAccessor.count);

                SGLTFModel::FillVertexAttribute(primitive, input, "POSITION", TINYGLTF_TYPE_VEC3, &bufferPos, posByteStride);
                SGLTFModel::FillVertexAttribute(primitive, input, "NORMAL", TINYGLTF_TYPE_VEC3, &bufferNormals, normByteStride);
                SGLTFModel::FillVertexAttribute(primitive, input, "TEXCOORD_0", TINYGLTF_TYPE_VEC2, &bufferTexCoordSet0, uv0ByteStride);
                SGLTFModel::FillVertexAttribute(primitive, input, "TEXCOORD_1", TINYGLTF_TYPE_VEC2, &bufferTexCoordSet1, uv1ByteStride);
                SGLTFModel::FillVertexAttribute(primitive, input, "JOINTS_0", TINYGLTF_TYPE_VEC4, &bufferJoints, jointByteStride);
                SGLTFModel::FillVertexAttribute(primitive, input, "JOINTS_0", TINYGLTF_TYPE_VEC4, &bufferWeights, weightByteStride);

                posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
                posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

                hasSkin = (bufferJoints && bufferWeights);

                for (size_t v = 0; v < posAccessor.count; ++v) {
                    Vertex vert = {};
                    vert.pos = glm::vec4(glm::make_vec3(&bufferPos[v * posByteStride]), 1.0f);
                    vert.normal = glm::vec4(glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : glm::vec3(0.0f))), 1.0f);
                    glm::vec2 uv0 = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : glm::vec2(0.0f);
                    glm::vec2 uv1 = bufferTexCoordSet1 ? glm::make_vec2(&bufferTexCoordSet1[v * uv1ByteStride]) : glm::vec2(0.0f);
                    vert.uv = glm::vec4(uv0.x, uv0.y, uv1.x, uv1.y);

                    /*
                    vert.joint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * jointByteStride])) : glm::vec4(0.0f);
                    vert.weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * weightByteStride]) : glm::vec4(0.0f);
                    // Fix for all zero weights
                    if (glm::length(vert.weight0) == 0.0f) {
                        vert.weight0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    }
                    */
                    vertexBuffer.push_back(vert);
                }
            }
            // Indices
            if (hasIndices) {
                const tinygltf::Accessor& accessor = input.accessors[primitive.indices > -1 ? primitive.indices : 0];
                const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

                indexCount = static_cast<uint32_t>(accessor.count);
                const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(buf[index] + vertexStart);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(buf[index] + vertexStart);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(buf[index] + vertexStart);
                    }
                    break;
                }
                default:
                    std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                    return;
                }
            }

            // loading last material as default one
            std::unique_ptr<Primitive> newPrimitive = std::make_unique<Primitive>(indexStart, vertexStart, indexCount, vertexCount,
                primitive.material > -1 ? *materials[primitive.material] : *materials.back());
            newPrimitive->bbox = AABBox(posMin, posMax);

            CreatePrimitiveBuffers(newPrimitive.get(), vertexBuffer, indexBuffer);

            newMesh->primitives.push_back(std::move(newPrimitive));
        }

        // Mesh BB from BBs of primitives
        for (const auto& p : newMesh->primitives) {
            if (p->bbox.valid && !newMesh->bbox.valid) {
                newMesh->bbox = p->bbox;
                newMesh->bbox.valid = true;
            }
            newMesh->bbox.min = glm::min(newMesh->bbox.min, p->bbox.min);
            newMesh->bbox.max = glm::max(newMesh->bbox.max, p->bbox.max);
        }
        newNode->mesh = std::move(newMesh);
    }
    if (parent) {
        parent->children.push_back(std::move(newNode));
        allNodes.push_back(parent->children.back().get());
    } else {
        nodes.push_back(std::move(newNode));
        allNodes.push_back(nodes.back().get());
    }
}

CG::Vk::GLTFModel::Mesh::Mesh(Device* vkDevice, const glm::mat4& meshMat)
{
    uniformBlock.matrix = meshMat;

    VK_CHECK_RESULT(vkDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &uniformBuffer.buffer,
        sizeof(uniformBlock)));

    uniformBuffer.buffer.Map(sizeof(uniformBlock));
    uniformBuffer.buffer.SetupDescriptor(sizeof(uniformBlock));
    UpdateUniformBuffers();
}

void CG::Vk::GLTFModel::Mesh::UpdateUniformBuffers()
{
    uniformBuffer.buffer.CopyTo(&uniformBlock, sizeof(uniformBlock));
}

glm::mat4 CG::Vk::GLTFModel::Node::GetLocalMatrix()
{
    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

glm::mat4 CG::Vk::GLTFModel::Node::GetWorldMatrix()
{
    glm::mat4 localMat = GetLocalMatrix();
    Node* currentParent = parent;
    while (currentParent) {
        localMat = currentParent->GetLocalMatrix() * localMat;
        currentParent = currentParent->parent;
    }
    return localMat;
}

void CG::Vk::GLTFModel::Node::UpdateRecursive()
{
    if (mesh) {
        glm::mat4 worldMat = GetWorldMatrix();
        if (skin) {
            mesh->uniformBlock.matrix = worldMat;

            glm::mat4 inverseTransform = glm::inverse(worldMat);
            size_t numJoints = std::min((uint32_t)skin->joints.size(), kMaxJointsCount);
            for (size_t i = 0; i < numJoints; i++) {
                Node* jointNode = skin->joints[i];
                glm::mat4 jointMat = jointNode->GetWorldMatrix() * skin->inverseBindMatrices[i];
                jointMat = inverseTransform * jointMat;
                mesh->uniformBlock.jointMatrix[i] = jointMat;
            }
            mesh->uniformBlock.jointcount = static_cast<float>(numJoints);
            mesh->UpdateUniformBuffers();
        } else {
            mesh->uniformBuffer.buffer.CopyTo(&worldMat, sizeof(glm::mat4));
        }
    }

    for (auto& child : children) {
        child->UpdateRecursive();
    }
}

void CG::Vk::GLTFModel::Texture::FromGLTFImage(const tinygltf::Image& glTFImage, TextureSampler textureSampler, Device* device, VkQueue copyQueue)
{
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

        texture.FromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height,
            device, copyQueue, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_TILING_OPTIMAL, textureSampler);
    } else {
        const unsigned char* buffer = &glTFImage.image[0];
        bufferSize = glTFImage.image.size();

        texture.FromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height,
            device, copyQueue, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_TILING_OPTIMAL, textureSampler);
    }
}

CG::Vk::GLTFModel::AABBox CG::Vk::GLTFModel::AABBox::GetAABB(const glm::mat4& m)
{
    glm::vec3 locMin = glm::vec3(m[3]);
    glm::vec3 locaMax = locMin;
    glm::vec3 v0, v1;

    glm::vec3 right = glm::vec3(m[0]);
    v0 = right * this->min.x;
    v1 = right * this->max.x;
    locMin += glm::min(v0, v1);
    locaMax += glm::max(v0, v1);

    glm::vec3 up = glm::vec3(m[1]);
    v0 = up * this->min.y;
    v1 = up * this->max.y;
    locMin += glm::min(v0, v1);
    locaMax += glm::max(v0, v1);

    glm::vec3 back = glm::vec3(m[2]);
    v0 = back * this->min.z;
    v1 = back * this->max.z;
    locMin += glm::min(v0, v1);
    locaMax += glm::max(v0, v1);

    return AABBox(locMin, locaMax);
}
