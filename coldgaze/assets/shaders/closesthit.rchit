#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct RayPayload
{
	vec4 colorAndDistance; // rgb + t
	vec4 scatterDirection; // xyz + w (is scatter needed)
	uint randomSeed;
};

struct VertexData
{
    vec3 inPos;
    vec3 inNormal;
    vec2 inUV0;
    vec2 inUV1;
    // inJoint0;
    // inWeight0;
};

struct Material {
	vec4 baseColorFactor;
	vec4 emissiveFactor;
	vec4 diffuseFactor;
	vec4 specularFactor;
    // int
	float workflow;
	int baseColorTextureSet;
	int physicalDescriptorTextureSet;
	int normalTextureSet;	
	int occlusionTextureSet;
	int emissiveTextureSet;
	float metallicFactor;	
	float roughnessFactor;
    // int
	float alphaMask;	
	float alphaMaskCutoff;
};

layout(binding = 0, set = 1) readonly buffer VertexBuffers { VertexData vertices[]; } vertexBuffers[];
layout(binding = 1, set = 1) readonly buffer IndexBuffers { uint indices[]; } indexBuffers[];
layout(binding = 2, set = 1) uniform sampler2D baseColorTextures[];
// layout(binding = 3, set = 1) readonly buffer Material materials[];

layout(location = 0) rayPayloadInNV RayPayload rayPayload;
hitAttributeNV vec3 attribs;

vec2 BaryLerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

VertexData FetchVertexData(uint offset)
{
    const uint index = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].indices[gl_PrimitiveID * 3 + offset];
    return vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].vertices[index];
}

void main()
{
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
      
    const VertexData v0 = FetchVertexData(0);
    const VertexData v1 = FetchVertexData(1);
    const VertexData v2 = FetchVertexData(2);
    
    const vec2 texCoord = BaryLerp(v0.inUV0.xy, v1.inUV0.xy, v2.inUV0.xy, barycentrics);
    
    rayPayload.colorAndDistance.xyz = vec3(texCoord, 1.0f);
}
