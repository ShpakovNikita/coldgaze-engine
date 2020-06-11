#version 460
#extension GL_EXT_control_flow_attributes : require
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct RayPayload
{
	vec3 color;
	vec3 scatterDirection;
    float rayDistance;
    bool isScattered;
	uint randomSeed;
};

struct VertexData
{
    vec4 inPos;
    vec4 inNormal;
    vec4 inUV; // xy = UV0, zw = UV1 
    // inJoint0;
    // inWeight0;
};

struct Material {
	vec4 baseColorFactor;
	vec4 emissiveFactor;
	vec4 diffuseFactor;
	vec4 specularFactor;
	float workflow;
	int baseColorTextureSet;
	int physicalDescriptorTextureSet;
	int normalTextureSet;	
	int occlusionTextureSet;
	int emissiveTextureSet;
	float metallicFactor;	
	float roughnessFactor;
	float alphaMask;	
	float alphaMaskCutoff;
};

struct PBRParams {
    float occlusion;              // pre calculated occlusion value on the surface
	float roughness;              // roughness value at the surface
	float metallic;               // metallic value at the surface
	vec4 albedo;                  // base albedo color value
	vec3 worldNormal;                // point on surface world position
};

layout(binding = 2, set = 0) uniform UBOScene
{
	mat4 projection;
	mat4 model;
	mat4 view;
    vec4 cameraPos;
    
    mat4 invProjection;
	mat4 invView;
    
    vec4 globalLightDir;
    vec4 globalLightColor;
} uboScene;

layout(binding = 0, set = 1) readonly buffer VertexBuffers { VertexData vertices[]; } vertexBuffers[];
layout(binding = 1, set = 1) readonly buffer IndexBuffers { uint indices[]; } indexBuffers[];
layout(binding = 2, set = 1) uniform MaterialBuffers { Material material; } materialBuffers[];
layout(binding = 3, set = 1) uniform sampler2D baseColorTextures[];
layout(binding = 4, set = 1) uniform sampler2D physicalDescriptorTextures[];
layout(binding = 5, set = 1) uniform sampler2D normalTextures[];
layout(binding = 6, set = 1) uniform sampler2D ambientOcclusionTextures[];
layout(binding = 7, set = 1) uniform sampler2D emissiveTextures[];

layout(binding = 0, set = 2) uniform sampler2D equirectangularMap;

layout(location = 0) rayPayloadInNV RayPayload rayPayload;
hitAttributeNV vec3 attribs;

const float PBR_WORKFLOW_METALLIC_ROUGHNESS = 0.0;
const float PBR_WORKFLOW_SPECULAR_GLOSINESS = 1.0;

const float c_MinRoughness = 0.04;

const float DIELECTRIC_REFLECTION_APPROXIMATION = 0.04;
const float PI = 3.14159265359;

const float GLASS_REFRACTION_INDEX = 1.517;


uint InitRandomSeed(uint val0, uint val1)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[[unroll]] 
	for (uint n = 0; n < 16; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

uint RandomInt(inout uint seed)
{
	// LCG values from Numerical Recipes
    return (seed = 1664525 * seed + 1013904223);
}

float RandomFloat(inout uint seed)
{
	// Float version using bitmask from Numerical Recipes
	const uint one = 0x3f800000;
	const uint msk = 0x007fffff;
	return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
}

vec2 RandomInUnitDisk(inout uint seed)
{
	for (;;)
	{
		const vec2 p = 2 * vec2(RandomFloat(seed), RandomFloat(seed)) - 1;
		if (dot(p, p) < 1)
		{
			return p;
		}
	}
}

vec3 RandomInUnitSphere(inout uint seed)
{
	for (;;)
	{
		const vec3 p = 2 * vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1;
		if (dot(p, p) < 1)
		{
			return p;
		}
	}
}

// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float FresnelSchlick(float cosTheta, float refractionIndex)
{
	float r0 = (1.0 - refractionIndex) / (1.0 + refractionIndex);
	r0 *= r0;
	return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}

// Lambertian
RayPayload ScatterLambertian(Material material, vec3 direction, PBRParams pbrParams, VertexData vertexData, float distance, inout uint seed)
{
	const bool isScattered = dot(direction, pbrParams.worldNormal) < 0.0;
    
    const vec3 color = pbrParams.albedo.rgb; 
	const vec3 scatter = pbrParams.worldNormal + RandomInUnitSphere(seed);

	return RayPayload(color, scatter, distance, isScattered, seed);
}

// Metallic
RayPayload ScatterMetallic(Material material, vec3 direction, PBRParams pbrParams, VertexData vertexData, float distance, inout uint seed)
{
	const vec3 reflected = reflect(direction, pbrParams.worldNormal);
    
    // TODO: is always true?
	const bool isScattered = dot(reflected, pbrParams.worldNormal) > 0;

	const vec3 color = isScattered ? pbrParams.albedo.rgb : vec3(1, 1, 1);
	const vec3 scatter = reflected + pbrParams.roughness * RandomInUnitSphere(seed);

	return RayPayload(color, scatter, distance, isScattered, seed);
}

// Dielectric
RayPayload ScatterDieletric(Material material, vec3 direction, PBRParams pbrParams, VertexData vertexData, float distance, inout uint seed)
{
	const float dot = dot(direction, pbrParams.worldNormal);
	const vec3 outwardNormal = dot > 0 ? -pbrParams.worldNormal : pbrParams.worldNormal;
	const float niOverNt = dot > 0 ? GLASS_REFRACTION_INDEX: 1 / GLASS_REFRACTION_INDEX;
	const float cosTheta = dot > 0 ? GLASS_REFRACTION_INDEX * dot : -dot;

	const vec3 refracted = refract(direction, outwardNormal, niOverNt);
	const float reflectProb = refracted != vec3(0) ? FresnelSchlick(cosTheta, GLASS_REFRACTION_INDEX) : 1;
	
	return RandomFloat(seed) < reflectProb
		? RayPayload(pbrParams.albedo.rgb, reflect(direction, pbrParams.worldNormal), distance, true, seed)
		: RayPayload(pbrParams.albedo.rgb, refracted, distance, true, seed);
}

// Diffuse Light
RayPayload ScatterDiffuseLight(Material material, float distance, inout uint seed)
{
	const vec3 scatter = vec3(1, 0, 0);

	return RayPayload(material.baseColorFactor.rgb, scatter, distance, false, seed);
}

RayPayload Scatter(Material material, vec3 direction, VertexData vertexData, float distance, inout uint seed, PBRParams pbrParams)
{
	const vec3 normDirection = normalize(direction);
    // return ScatterLambertian(material, normDirection, pbrParams, vertexData, distance, seed);
    return ScatterMetallic(material, normDirection, pbrParams, vertexData, distance, seed);
    return ScatterDiffuseLight(material, distance, seed);
    return ScatterDieletric(material, normDirection, pbrParams, vertexData, distance, seed);
	
}

vec2 dFdy(vec2 p)
{
    return vec2(p.x, p.y + 0.01) - p;
}

vec3 dFdy(vec3 p)
{
    return vec3(p.x, p.y + 0.01, p.z) - p;
}

vec2 dFdx(vec2 p)
{
    return vec2(p.x + 0.01, p.y) - p;
}

vec3 dFdx(vec3 p)
{
    return vec3(p.x + 0.01, p.yz) - p;
}

// More info http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal(VertexData vertexData, Material material, vec3 worldPos)
{
    vec3 worldNormal = normalize(gl_ObjectToWorldNV * vertexData.inNormal);
    
	vec3 tangentNormal;
    vec2 inUV;
 
    if (material.normalTextureSet > -1) {
        inUV = material.normalTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw;
        tangentNormal = texture(normalTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], inUV).xyz * 2.0 - 1.0;
    } else {
        return worldNormal;
    }

	vec3 q1 = dFdx(worldPos);
	vec3 q2 = dFdy(worldPos);
	vec2 st1 = dFdx(inUV);
	vec2 st2 = dFdy(inUV);

	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 N = normalize(worldNormal);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

vec2 BaryLerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec4 BaryLerp(vec4 a, vec4 b, vec4 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

VertexData BaryLerp(VertexData v0, VertexData v1, VertexData v2, vec3 barycentrics)
{
    VertexData outData;
    outData.inUV = BaryLerp(v0.inUV, v1.inUV, v2.inUV, barycentrics);
    outData.inPos = BaryLerp(v0.inPos, v1.inPos, v2.inPos, barycentrics);
    outData.inNormal = BaryLerp(v0.inNormal, v1.inNormal, v2.inNormal, barycentrics);
    
    return outData;
}

VertexData FetchVertexData(uint offset)
{
    const uint index = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].indices[gl_PrimitiveID * 3 + offset];
    return vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].vertices[index];
}

PBRParams GetPBRParams(VertexData vertexData, Material material)
{
    vec4 albedo;
    if (material.baseColorTextureSet > -1) {
        albedo = texture(baseColorTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], 
            material.baseColorTextureSet == 0 ? vertexData.inUV.xy :  vertexData.inUV.zw);
    } else {
        albedo = material.baseColorFactor;
    }

    vec3 worldPos = gl_WorldRayOriginNV + gl_HitTNV * gl_WorldRayDirectionNV;
    
    vec3 N = perturbNormal(vertexData, material, worldPos);
    
    float occlusion = 1.0f;
    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    
    if (material.physicalDescriptorTextureSet > -1) {
        vec4 mrSample = texture(physicalDescriptorTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], material.physicalDescriptorTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw);
        roughness = mrSample.g * roughness;
        metallic = mrSample.b * metallic;
    } else {
        roughness = clamp(roughness, c_MinRoughness, 1.0);
        metallic = clamp(metallic, 0.0, 1.0);
    }
    
    if (material.occlusionTextureSet > -1) {
        occlusion = texture(ambientOcclusionTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], (material.occlusionTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw)).r;
    }
    
    PBRParams pbrParams = PBRParams(
        occlusion,
        roughness,
        metallic,
        albedo,
        N
    );
    
    return pbrParams;
}

void main()
{
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
      
    const VertexData v0 = FetchVertexData(0);
    const VertexData v1 = FetchVertexData(1);
    const VertexData v2 = FetchVertexData(2);
    
    const VertexData vertexData = BaryLerp(v0, v1, v2, barycentrics);
    const Material material = materialBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].material;
    
    PBRParams pbrParams = GetPBRParams(vertexData, material);
    rayPayload = Scatter(material, gl_WorldRayDirectionNV, vertexData, gl_HitTNV, rayPayload.randomSeed, pbrParams);
}
